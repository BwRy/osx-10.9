/*
 * Copyright (c) 2012-2013 Apple Inc. All rights reserved.
 */
#include <notify.h>

#include <SystemConfiguration/SCPrivate.h>
#include <SystemConfiguration/VPNAppLayerPrivate.h>
#include <Security/Security.h>

#include <netinet/flow_divert_proto.h>

#include "scnc_main.h"
#include "scnc_utils.h"

#include "app_layer.h"
#include "flow_divert_controller.h"

#ifndef FLOW_DIVERT_DNS_SERVICE_SIGNING_ID
#define FLOW_DIVERT_DNS_SERVICE_SIGNING_ID		"com.apple.mDNSResponder"
#endif /* FLOW_DIVERT_DNS_SERVICE_SIGNING_ID */

static CFStringRef g_dynamic_store_key = NULL;

static int
app_layer_get_first_enabled_service_rank(SCPreferencesRef prefs, CFStringRef rule_id, CFDictionaryRef rules_hash, CFArrayRef service_order)
{
	CFArrayRef match_services;
	CFDictionaryRef rule;
	CFIndex rank = -1;

	rule = CFDictionaryGetValue(rules_hash, rule_id);
	if (isDictionary(rule)) {
		CFIndex idx;
		match_services = CFDictionaryGetValue(rule, kVPNAppLayerRuleMatchServices);
		for (idx = 0; idx < CFArrayGetCount(match_services); idx++) {
			CFDictionaryRef match_service = CFArrayGetValueAtIndex(match_services, idx);
			CFStringRef service_id = CFDictionaryGetValue(match_service, kVPNAppLayerRuleMatchServiceID);
			SCNetworkServiceRef service = SCNetworkServiceCopy(prefs, service_id);
			if (service != NULL) {
				if (SCNetworkServiceGetEnabled(service)) {
					if (service_order != NULL) {
						rank = CFArrayGetFirstIndexOfValue(service_order, CFRangeMake(0, CFArrayGetCount(service_order)), service_id);
					} else {
						rank = idx;
					}
				}
				CFRelease(service);
				if (rank >= 0) {
					break;
				}
			}
		}
	}

	return rank;
}


static CFIndex
app_layer_compare_rules(SCPreferencesRef prefs, CFStringRef rule_id1, CFStringRef rule_id2, CFDictionaryRef rules_hash, CFArrayRef service_order)
{
	CFIndex rank1 = -1;
	CFIndex rank2 = -1;

	if (CFEqual(rule_id1, rule_id2)) {
		return 0;
	}

	rank1 = app_layer_get_first_enabled_service_rank(prefs, rule_id1, rules_hash, service_order);
	rank2 = app_layer_get_first_enabled_service_rank(prefs, rule_id2, rules_hash, service_order);

	if (rank1 >= 0 && rank2 >= 0) {
		return (rank2 - rank1);
	} else {
		return rank1 - rank2;
	}
}


static void
app_layer_add_executable(SCPreferencesRef prefs, CFMutableDictionaryRef config, CFStringRef rule_id, CFArrayRef exec_infos, CFDictionaryRef rules, CFArrayRef service_order)
{
	CFIndex exec_idx;
	CFIndex exec_info_count = CFArrayGetCount(exec_infos);
	CFMutableDictionaryRef executables =
		(CFMutableDictionaryRef)CFDictionaryGetValue(config, kVPNAppLayerMatchExecutables);

	/* Create the signing identifier hash if it doesn't already exist */
	if (!isDictionary(executables)) {
		executables = CFDictionaryCreateMutable(kCFAllocatorDefault,
		                                        0,
		                                        &kCFTypeDictionaryKeyCallBacks,
		                                        &kCFTypeDictionaryValueCallBacks);
		CFDictionarySetValue(config, kVPNAppLayerMatchExecutables, executables);
		CFRelease(executables);
	}

	/* Create an entry in the signing identifier hash for each set of executable info in the rule */
	for (exec_idx = 0; exec_idx < exec_info_count; exec_idx++) {
		CFDictionaryRef exec_info = CFArrayGetValueAtIndex(exec_infos, exec_idx);
		CFStringRef signing_id = CFDictionaryGetValue(exec_info, kSCValNetVPNAppRuleExecutableSigningIdentifier);

		if (isString(signing_id)) {
			Boolean do_insert = TRUE;

			CFDictionaryRef existing_match = CFDictionaryGetValue(executables, signing_id);
			if (isDictionary(existing_match)) {
				CFStringRef existing_rule_id = CFDictionaryGetValue(existing_match, kSCValNetVPNAppRuleIdentifier);
				if (isString(existing_rule_id) && app_layer_compare_rules(prefs, rule_id, existing_rule_id, rules, service_order) < 0) {
					do_insert = FALSE;
				}
			}

			if (do_insert) {
				CFStringRef designated_req = CFDictionaryGetValue(exec_info, kSCValNetVPNAppRuleExecutableDesignatedRequirement);
				CFMutableDictionaryRef new_exec_match = CFDictionaryCreateMutable(kCFAllocatorDefault,
				                                                                  0,
				                                                                  &kCFTypeDictionaryKeyCallBacks,
				                                                                  &kCFTypeDictionaryValueCallBacks);

				CFDictionarySetValue(new_exec_match, kSCValNetVPNAppRuleIdentifier, rule_id);
				if (isString(designated_req)) {
#if ! TARGET_OS_EMBEDDED
					SecRequirementRef requirement = NULL;
					OSStatus req_status;

					req_status = SecRequirementCreateWithString(designated_req, kSecCSDefaultFlags, &requirement);
					if (req_status != errSecSuccess) {
						SCLog(TRUE, LOG_WARNING, CFSTR("Discarding executable with invalid designated requirement: %@"), designated_req);
						do_insert = FALSE;
					}

					if (requirement != NULL) {
						CFRelease(requirement);
					} 
#endif /* ! TARGET_OS_EMBEDDED */
					CFDictionarySetValue(new_exec_match, kSCValNetVPNAppRuleExecutableDesignatedRequirement, designated_req);
				}
#if ! TARGET_OS_EMBEDDED
			   	else {
					SCLog(TRUE, LOG_WARNING, CFSTR("Discarding executable with missing designated requirement: %@"), exec_info);
					do_insert = FALSE;
				}
#endif /* ! TARGET_OS_EMBEDDED */

				if (do_insert) {
					CFDictionarySetValue(executables, signing_id, new_exec_match);
				}

				CFRelease(new_exec_match);
			}
		}
	}

	SCLog(gSCNCDebug, LOG_INFO, CFSTR("Executables: %@"), executables);
}

static void
app_layer_add_account(SCPreferencesRef prefs, CFMutableDictionaryRef config, CFStringRef rule_id, CFArrayRef account_infos, CFDictionaryRef rules, CFArrayRef service_order)
{
	CFMutableDictionaryRef accounts = (CFMutableDictionaryRef)CFDictionaryGetValue(config, kVPNAppLayerMatchAccounts);
	
	if (account_infos != NULL) {
		CFIndex account_idx;
		CFIndex num_accounts = CFArrayGetCount(account_infos);
		if (num_accounts != 0) {
			if (!isDictionary(accounts)) {
				accounts = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
				CFDictionarySetValue(config, kVPNAppLayerMatchAccounts, accounts);
				CFRelease(accounts);
			}
			
			for (account_idx = 0; account_idx < num_accounts; account_idx++) {
				CFStringRef account_identifier = CFArrayGetValueAtIndex(account_infos, account_idx);
				if (isString(account_identifier)) {
					CFStringRef existing_rule_ID = CFDictionaryGetValue(accounts, account_identifier);
					if (!isString(existing_rule_ID) || !(app_layer_compare_rules(prefs, rule_id, existing_rule_ID, rules, service_order) < 0)) {
						CFDictionarySetValue(accounts, account_identifier, rule_id);
					}
				}
			}
		}
	}
}

#ifndef kSCValNetVPNAppRuleAccountIdentifierMatch
#define kSCValNetVPNAppRuleAccountIdentifierMatch CFSTR("AccountIdentifierMatch")
#endif

static void
app_layer_add_service(SCPreferencesRef prefs, CFMutableDictionaryRef config, SCNetworkServiceRef scservice, CFArrayRef service_order)
{
	CFMutableDictionaryRef rules = (CFMutableDictionaryRef)CFDictionaryGetValue(config, kVPNAppLayerRules);
	CFIndex rule_count;
	CFArrayRef rule_ids;
	CFIndex rule_idx;
	CFStringRef service_id = SCNetworkServiceGetServiceID(scservice);
	CFIndex service_rank = 0;

	rule_ids = VPNServiceCopyAppRuleIDs(scservice);
	if (rule_ids == NULL) {
		return;
	}

	if (service_order != NULL) {
		service_rank = CFArrayGetFirstIndexOfValue(service_order, CFRangeMake(0, CFArrayGetCount(service_order)), service_id);
	}

	rule_count = CFArrayGetCount(rule_ids);
	for (rule_idx = 0; rule_idx < rule_count; rule_idx++) {
		CFStringRef rule_ID = CFArrayGetValueAtIndex(rule_ids, rule_idx);
		CFDictionaryRef rule_settings = VPNServiceCopyAppRule(scservice, rule_ID);
		if (rule_settings != NULL) {
			CFArrayRef rule_match_domains = CFDictionaryGetValue(rule_settings, kSCValNetVPNAppRuleDNSDomainMatch);
			CFArrayRef rule_match_executables = CFDictionaryGetValue(rule_settings, kSCValNetVPNAppRuleExecutableMatch);
			CFArrayRef rule_match_accounts = CFDictionaryGetValue(rule_settings, kSCValNetVPNAppRuleAccountIdentifierMatch);
			CFMutableArrayRef match_services = NULL;
			CFIndex service_insert_idx = -1;
			Boolean replace = FALSE;
			CFMutableDictionaryRef new_match_service;

			if (isDictionary(rules)) {
				CFDictionaryRef rule = CFDictionaryGetValue(rules, rule_ID);
				if (isDictionary(rule)) {
					match_services = (CFMutableArrayRef)CFDictionaryGetValue(rule, kVPNAppLayerRuleMatchServices);
					if (isArray(match_services)) {
						CFIndex service_count = CFArrayGetCount(match_services);

						if (SCNetworkServiceGetEnabled(scservice)) {
							CFIndex service_idx;
							/* Figure out where to insert this service into the rule's list of services */
							for (service_idx = 0; service_idx < service_count; service_idx++) {
								CFDictionaryRef match_service = CFArrayGetValueAtIndex(match_services, service_idx);
								if (isDictionary(match_service)) {
									CFStringRef match_sid = CFDictionaryGetValue(match_service, kVPNAppLayerRuleMatchServiceID);

									if (CFEqual(match_sid, service_id)) {
										service_insert_idx = service_idx;
										replace = TRUE;
										break;
									} else {
										CFIndex rank = 0;
										if (service_order != NULL) {
											rank = CFArrayGetFirstIndexOfValue(service_order,
											                                   CFRangeMake(0, CFArrayGetCount(service_order)),
											                                   match_sid);
										}
										if (service_rank >= 0 && rank > service_rank) {
											service_insert_idx = service_idx;
											break;
										}
									}
								}
							}
						}

						if (service_insert_idx < 0) {
							/* Put it at the end of the list */
							service_insert_idx = service_count;
						}
					} else {
						match_services = NULL;
					}
				}
			}

			new_match_service = CFDictionaryCreateMutable(kCFAllocatorDefault,
					0,
					&kCFTypeDictionaryKeyCallBacks,
					&kCFTypeDictionaryValueCallBacks);

			CFDictionarySetValue(new_match_service, kVPNAppLayerRuleMatchServiceID, service_id);
			if (rule_match_domains != NULL) {
				CFDictionarySetValue(new_match_service, kVPNAppLayerRuleMatchServiceDomains, rule_match_domains);
			}

			if (service_insert_idx >= 0 && isArray(match_services)) {
				if (replace) {
					CFArraySetValueAtIndex(match_services, service_insert_idx, new_match_service);
				} else {
					CFArrayInsertValueAtIndex(match_services, service_insert_idx, new_match_service);
				}
			} else {
				/* Here if we did not find an existing rule corresponding to this rule */
				CFMutableDictionaryRef new_rule = CFDictionaryCreateMutable(kCFAllocatorDefault,
				                                                            0,
				                                                            &kCFTypeDictionaryKeyCallBacks,
				                                                            &kCFTypeDictionaryValueCallBacks);

				match_services = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);
				CFArrayAppendValue(match_services, new_match_service);

				CFDictionarySetValue(new_rule, kVPNAppLayerRuleMatchServices, match_services);

				if (!isDictionary(rules)) {
					rules = CFDictionaryCreateMutable(kCFAllocatorDefault,
					                                  0,
					                                  &kCFTypeDictionaryKeyCallBacks,
					                                  &kCFTypeDictionaryValueCallBacks);
					CFDictionarySetValue(config, kVPNAppLayerRules, rules);
					CFRelease(rules);
				}

				CFDictionarySetValue(rules, rule_ID, new_rule);

				CFRelease(new_rule);
				CFRelease(match_services);
			}

			if (rule_match_executables) {
				app_layer_add_executable(prefs, config, rule_ID, rule_match_executables, rules, service_order);
			}

			if (rule_match_accounts) {
				app_layer_add_account(prefs, config, rule_ID, rule_match_accounts, rules, service_order);
			}
			
			CFRelease(new_match_service);
			CFRelease(rule_settings);
		}
	}

	CFRelease(rule_ids);
}

static void
app_layer_post_config_changed_notification(CFIndex rule_count)
{
	static int notify_token = -1;
	uint32_t status;
	uint64_t state = rule_count;

	if (notify_token == -1) {
		status = notify_register_check(kVPNAPPLAYER_NOTIFY_KEY, &notify_token);
		if (status != NOTIFY_STATUS_OK) {
			SCLog(TRUE, LOG_ERR, CFSTR("app_layer_post_config_changed_notification: notify_register_check failed, status = %d"), status);
			return;
		}
	}

	status = notify_set_state(notify_token, state);
	if (status != NOTIFY_STATUS_OK) {
		SCLog(TRUE, LOG_ERR, CFSTR("app_layer_post_config_changed_notification: notify_set_state failed, status = %d"), status);
		notify_cancel(notify_token);
		notify_token = -1;
		return;
	}

	status = notify_post(kVPNAPPLAYER_NOTIFY_KEY);
	if (status != NOTIFY_STATUS_OK) {
		SCLog(TRUE, LOG_ERR, CFSTR("app_layer_post_config_changed_notification: notify_post failed, status = %d"), status);
		notify_cancel(notify_token);
		notify_token = -1;
		return;
	}
}


void
app_layer_remove_app(CFStringRef signing_id)
{
	SCPreferencesRef prefs = SCPreferencesCreate(kCFAllocatorDefault, CFSTR("app_layer_remove_app"), NULL);
	if (prefs != NULL) {
		Boolean changed = FALSE;
		CFArrayRef services = VPNServiceCopyAll(prefs);
		if (isA_CFArray(services) && CFArrayGetCount(services) > 0) {
			CFIndex service_idx;
			for (service_idx = 0; service_idx < CFArrayGetCount(services); service_idx++) {
				SCNetworkServiceRef service = CFArrayGetValueAtIndex(services, service_idx);
				CFIndex rule_idx;
				CFArrayRef rule_ids = VPNServiceCopyAppRuleIDs(service);
				if (isArray(rule_ids)) {
					for (rule_idx = 0; rule_idx < CFArrayGetCount(rule_ids); rule_idx++) {
						CFStringRef rule_id = CFArrayGetValueAtIndex(rule_ids, rule_idx);
						CFDictionaryRef rule_settings = VPNServiceCopyAppRule(service, rule_id);
						if (isDictionary(rule_settings)) {
							CFArrayRef execs = CFDictionaryGetValue(rule_settings, kSCValNetVPNAppRuleExecutableMatch);
							if (isArray(execs)) {
								CFMutableArrayRef new_execs = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);
								CFIndex exec_idx;
								for (exec_idx = 0; exec_idx < CFArrayGetCount(execs); exec_idx++) {
									CFDictionaryRef exec = CFArrayGetValueAtIndex(execs, exec_idx);
									CFStringRef exec_signing_id =
										CFDictionaryGetValue(exec, kSCValNetVPNAppRuleExecutableSigningIdentifier);
									if (!CFEqual(exec_signing_id, signing_id)) {
										CFArrayAppendValue(new_execs, exec);
									}
								}
								if (CFArrayGetCount(execs) != CFArrayGetCount(new_execs)) {
									if (CFArrayGetCount(new_execs) == 0) {
										if (VPNServiceRemoveAppRule(service, rule_id)) {
											changed = TRUE;
										} else {
											SCLog(TRUE, LOG_WARNING, CFSTR("app_layer_remove_app failed to remove rule %@ from service %@: %s"), rule_id, SCNetworkServiceGetServiceID(service), SCErrorString(SCError()));
										}
									} else {
										CFMutableDictionaryRef new_rule =
											CFDictionaryCreateMutableCopy(kCFAllocatorDefault, CFDictionaryGetCount(rule_settings), rule_settings);
										CFDictionarySetValue(new_rule, kSCValNetVPNAppRuleExecutableMatch, new_execs);
										if (VPNServiceSetAppRule(service, rule_id, new_rule)) {
											changed = TRUE;
										} else {
											SCLog(TRUE, LOG_WARNING, CFSTR("app_layer_remove_app failed to set rule %@ from service %@: %s"), rule_id, SCNetworkServiceGetServiceID(service), SCErrorString(SCError()));
										}
										CFRelease(new_rule);
									}
								}
								CFRelease(new_execs);
							}
						}
						if (rule_settings != NULL) {
							CFRelease(rule_settings);
						}
					}
				}
				if (rule_ids != NULL) {
					CFRelease(rule_ids);
				}
			}
		}

		if (changed) {
			if (!SCPreferencesCommitChanges(prefs)) {
				SCLog(TRUE, LOG_WARNING, CFSTR("app_layer_remove_app failed to commit changes while removing app %@: %s"), signing_id, SCErrorString(SCError()));
			}
			if (!SCPreferencesApplyChanges(prefs)) {
				SCLog(TRUE, LOG_WARNING, CFSTR("app_layer_remove_app failed to apply changes while removing app %@: %s"), signing_id, SCErrorString(SCError()));
			}
		}

		if (services != NULL) {
			CFRelease(services);
		}
		CFRelease(prefs);
	}
}


void
app_layer_prefs_changed(SCPreferencesRef prefs, SCPreferencesNotification notification_type, void *info)
{
#pragma unused(info)
	if (notification_type & kSCPreferencesNotificationApply) {
		SCNetworkSetRef current_set = SCNetworkSetCopyCurrent(prefs);
		CFDictionaryRef new_config = NULL;
		CFArrayRef service_order = NULL;
		CFArrayRef services = VPNServiceCopyAll(prefs);

		if (current_set != NULL) {
			service_order = SCNetworkSetGetServiceOrder(current_set);
		}

		if (services != NULL) {
			CFMutableDictionaryRef config = CFDictionaryCreateMutable(kCFAllocatorDefault,
			                                                          0,
			                                                          &kCFTypeDictionaryKeyCallBacks,
			                                                          &kCFTypeDictionaryValueCallBacks);
			CFIndex services_count = CFArrayGetCount(services);
			CFIndex idx;
			for (idx = 0; idx < services_count; idx++) {
				SCNetworkServiceRef service = (SCNetworkServiceRef)CFArrayGetValueAtIndex(services, idx);
				app_layer_add_service(prefs, config, service, service_order);
			}

			if (CFDictionaryGetCount(config) > 0) {
				new_config = CFDictionaryCreateCopy(kCFAllocatorDefault, config);
			}
			CFRelease(config);
		}

		if (new_config != NULL) {
			CFDictionaryRef curr_config = (CFDictionaryRef)SCDynamicStoreCopyValue(gDynamicStore, g_dynamic_store_key);
			if (!isDictionary(curr_config) || !CFEqual(new_config, curr_config)) {
				CFArrayRef signing_ids = NULL;
				CFDictionaryRef executables = CFDictionaryGetValue(new_config, kVPNAppLayerMatchExecutables);
				CFIndex exec_count = (executables != NULL ? CFDictionaryGetCount(executables) : 0);
				if (exec_count > 0) {
					CFStringRef *keys = CFAllocatorAllocate(kCFAllocatorDefault, (exec_count + 1) * sizeof(*keys), 0);
					CFDictionaryGetKeysAndValues(executables, (const void **)keys, NULL);
					keys[exec_count] = CFSTR(FLOW_DIVERT_DNS_SERVICE_SIGNING_ID);
					signing_ids = CFArrayCreate(kCFAllocatorDefault, (const void **)keys, exec_count + 1, &kCFTypeArrayCallBacks);
					CFAllocatorDeallocate(kCFAllocatorDefault, keys);
				}
				flow_divert_set_signing_ids(signing_ids);
				if (signing_ids != NULL) {
					CFRelease(signing_ids);
				}
#if ! TARGET_OS_EMBEDDED && ! TARGET_IPHONE_SIMULATOR
				VPNAppLayerUUIDPolicyClear();
#endif
				SCDynamicStoreSetValue(gDynamicStore, g_dynamic_store_key, new_config);
				app_layer_post_config_changed_notification(CFDictionaryGetCount(new_config));
			}
			CFRelease(new_config);
			if (curr_config != NULL) {
				CFRelease(curr_config);
			}
		} else {
			flow_divert_set_signing_ids(NULL);
			SCDynamicStoreRemoveValue(gDynamicStore, g_dynamic_store_key);
			app_layer_post_config_changed_notification(0);
		}

		if (current_set != NULL) {
			CFRelease(current_set);
		}
		if (services != NULL) {
			CFRelease(services);
		}

		SCPreferencesSynchronize(prefs);
	}
}

void
app_layer_init(CFRunLoopRef rl, CFStringRef rl_mode)
{
	static dispatch_once_t app_layer_initialized;

	dispatch_once(&app_layer_initialized, ^{
		SCPreferencesRef prefs = SCPreferencesCreate(kCFAllocatorDefault, CFSTR("app_layer_rules"), NULL);
		SCPreferencesContext prefs_ctx = { 0, NULL, NULL, NULL };

		if (!SCPreferencesSetCallback(prefs, app_layer_prefs_changed, &prefs_ctx)) {
			SCLog(TRUE, LOG_ERR, CFSTR("app_layer_init: failed to set the prefs callback: %s"), SCErrorString(SCError()));
			return;
		}

		if (!SCPreferencesScheduleWithRunLoop(prefs, rl, rl_mode)) {
			SCLog(TRUE, LOG_ERR, CFSTR("app_layer_init: failed to schedule the prefs with the run loop: %s"), SCErrorString(SCError()));
			return;
		}

		g_dynamic_store_key = SCDynamicStoreKeyCreateNetworkGlobalEntity(NULL, kSCDynamicStoreDomainState, kSCEntNetAppLayer);

		app_layer_prefs_changed(prefs, kSCPreferencesNotificationApply, NULL);

		CFRelease(prefs);
	});
}
