// policies.h - pick a policy by name. Policy 4 (adaptive) will be added in Milestone 3.
#pragma once
#include <string>

#include "policy_poll.h"
#include "policy_spin.h"
#include "policy_wait.h"

// name: "wait", "poll" or "spin". For "spin", spin_us is the fixed spin time in microseconds.
inline const Policy* pick_policy(const std::string& name, double spin_us = 0) {
    if (name == "wait") return &kPolicyWaitImmediately;
    if (name == "poll") return &kPolicyPoll;
    if (name == "spin") { g_spin_ns = static_cast<int64_t>(spin_us * 1000.0); return &kPolicySpin; }
    return nullptr;
}
