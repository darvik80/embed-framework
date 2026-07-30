/// Host Unity tests for embed::StateMachine.

#include "unity.h"

#include "embed/state_machine.hpp"

namespace {

// ── Events ──────────────────────────────────────────────────────────────

struct Connect {};
struct Disconnect {};
struct Timeout {};

// ── States ──────────────────────────────────────────────────────────────

struct Idle;
struct Connected;
struct Reconnecting;

struct Idle : embed::State<embed::On<Connect, Connected>> {};

struct Connected : embed::State<
    embed::On<Disconnect, Idle>,
    embed::On<Timeout, Reconnecting>> {};

struct Reconnecting : embed::State<
    embed::On<Connect, Connected>,
    embed::On<Disconnect, Idle>> {};

// ── Machine ─────────────────────────────────────────────────────────────

struct TestMachine : embed::StateMachine<TestMachine, Idle, Connected, Reconnecting> {
    int transitionCount = 0;
    int lastTarget = -1;  // 0=Idle, 1=Connected, 2=Reconnecting

    void onStateChanged(const embed::TransitionTo<Idle>&) {
        ++transitionCount; lastTarget = 0;
    }
    void onStateChanged(const embed::TransitionTo<Connected>&) {
        ++transitionCount; lastTarget = 1;
    }
    void onStateChanged(const embed::TransitionTo<Reconnecting>&) {
        ++transitionCount; lastTarget = 2;
    }
    void onStateChanged(const embed::Nothing&) {
        // no transition occurred
    }
};

} // namespace

extern "C" {

void setUp(void) {}
void tearDown(void) {}

void test_initial_state_is_first(void)
{
    TestMachine m;
    // Initial state is Idle (first in template params)
    TEST_ASSERT_TRUE(std::holds_alternative<Idle*>(m.getCurrentState()));
}

void test_transition_idle_to_connected(void)
{
    TestMachine m;
    m.handle(Connect{});
    TEST_ASSERT_TRUE(std::holds_alternative<Connected*>(m.getCurrentState()));
    TEST_ASSERT_EQUAL_INT(1, m.transitionCount);
    TEST_ASSERT_EQUAL_INT(1, m.lastTarget);
}

void test_transition_connected_to_idle(void)
{
    TestMachine m;
    m.handle(Connect{});
    m.handle(Disconnect{});
    TEST_ASSERT_TRUE(std::holds_alternative<Idle*>(m.getCurrentState()));
    TEST_ASSERT_EQUAL_INT(2, m.transitionCount);
    TEST_ASSERT_EQUAL_INT(0, m.lastTarget);
}

void test_transition_connected_to_reconnecting(void)
{
    TestMachine m;
    m.handle(Connect{});
    m.handle(Timeout{});
    TEST_ASSERT_TRUE(std::holds_alternative<Reconnecting*>(m.getCurrentState()));
    TEST_ASSERT_EQUAL_INT(2, m.lastTarget);
}

void test_reconnecting_to_connected(void)
{
    TestMachine m;
    m.handle(Connect{});
    m.handle(Timeout{});
    m.handle(Connect{});
    TEST_ASSERT_TRUE(std::holds_alternative<Connected*>(m.getCurrentState()));
    TEST_ASSERT_EQUAL_INT(3, m.transitionCount);
}

void test_unhandled_event_no_transition(void)
{
    TestMachine m;
    // Idle doesn't handle Disconnect or Timeout
    m.handle(Disconnect{});
    TEST_ASSERT_TRUE(std::holds_alternative<Idle*>(m.getCurrentState()));
    TEST_ASSERT_EQUAL_INT(0, m.transitionCount);
}

void test_prev_state_tracks(void)
{
    TestMachine m;
    m.handle(Connect{});
    TEST_ASSERT_TRUE(std::holds_alternative<Idle*>(m.getPrevState()));
    TEST_ASSERT_TRUE(std::holds_alternative<Connected*>(m.getCurrentState()));
}

void test_full_cycle(void)
{
    TestMachine m;
    m.handle(Connect{});      // Idle -> Connected
    m.handle(Timeout{});      // Connected -> Reconnecting
    m.handle(Disconnect{});   // Reconnecting -> Idle
    TEST_ASSERT_TRUE(std::holds_alternative<Idle*>(m.getCurrentState()));
    TEST_ASSERT_EQUAL_INT(3, m.transitionCount);
}

} // extern "C"

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_initial_state_is_first);
    RUN_TEST(test_transition_idle_to_connected);
    RUN_TEST(test_transition_connected_to_idle);
    RUN_TEST(test_transition_connected_to_reconnecting);
    RUN_TEST(test_reconnecting_to_connected);
    RUN_TEST(test_unhandled_event_no_transition);
    RUN_TEST(test_prev_state_tracks);
    RUN_TEST(test_full_cycle);
    return UNITY_END();
}
