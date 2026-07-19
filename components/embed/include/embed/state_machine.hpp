#pragma once

#include <cstdint>
#include <concepts>
#include <tuple>
#include <variant>

namespace embed {

// ── Transition types ────────────────────────────────────────────────────

/// Transition to a specific state.
template<typename State>
struct TransitionTo {
    template<typename Machine>
    TransitionTo& execute(Machine& machine) {
        machine.template transitionTo<State>();
        return *this;
    }
};

/// No state transition.
struct Nothing {
    template<typename Machine>
    Nothing& execute(Machine&) {
        return *this;
    }
};

// ── Concepts ────────────────────────────────────────────────────────────

namespace detail {

/// Check if type T is one of Types...
template<typename T, typename... Types>
concept one_of = (std::same_as<T, Types> || ...);

/// Check if a state class has a handle(Event) method returning
/// Nothing or TransitionTo<S> for some S in States...
template<typename StateType, typename Event, typename... States>
concept has_handle = requires(const StateType& state, const Event& event) {
    { state.handle(event) } -> one_of<Nothing, TransitionTo<States>...>;
};

} // namespace detail

// ── State machine ───────────────────────────────────────────────────────

/// Generic state machine with CRTP.
///
/// T is the derived type (for onStateChanged callbacks).
/// States... are the possible state types.
///
/// Usage:
///   class MyMachine : public StateMachine<MyMachine, StateA, StateB, StateC> {
///       void onStateChanged(const TransitionTo<StateB>&) { ... }
///       void onStateChanged(const Nothing&) { ... }
///   };
template<typename T, typename... States>
class StateMachine {
public:
    /// Transition to a specific state.
    template<typename State>
    void transitionTo() {
        prevState = currentState;
        currentState = &std::get<State>(states);
    }

    /// Handle an event in the current state.
    /// If the current state has handle(Event), calls it and
    /// invokes onStateChanged() on the derived class.
    template<typename Event>
    void handle(const Event& event) {
        auto passEventToState = [this, &event](auto statePtr) {
            if constexpr (detail::has_handle<decltype(*statePtr), Event, States...>) {
                static_cast<T*>(this)->onStateChanged(
                    statePtr->handle(event).execute(*this));
            }
        };
        std::visit(passEventToState, currentState);
    }

    /// Handle an event with extra context passed to onStateChanged.
    template<typename Event, typename Ctx>
    void handle(const Event& event, const Ctx& ctx) {
        auto passEventToState = [this, &event, &ctx](auto statePtr) {
            if constexpr (detail::has_handle<decltype(*statePtr), Event, States...>) {
                static_cast<T*>(this)->onStateChanged(
                    statePtr->handle(event).execute(*this), ctx);
            }
        };
        std::visit(passEventToState, currentState);
    }

    /// Get the current state variant.
    [[nodiscard]] const std::variant<States*...>& getCurrentState() const {
        return currentState;
    }

    /// Get the previous state variant.
    [[nodiscard]] const std::variant<States*...>& getPrevState() const {
        return prevState;
    }

private:
    std::tuple<States...> states;
    std::variant<States*...> currentState{&std::get<0>(states)};
    std::variant<States*...> prevState{&std::get<0>(states)};
};

// ── Transition descriptor ───────────────────────────────────────────────

/// Describes a single Event -> TargetState mapping.
template<typename Event, typename TargetState>
struct On {
    using event_type  = Event;
    using target_type = TargetState;
};

// ── State handler mixin ─────────────────────────────────────────────────

namespace detail {

/// Mixin that provides one handle() method for a single On<E, S> descriptor.
template<typename Transition>
struct StateHandler {
    [[nodiscard]] TransitionTo<typename Transition::target_type>
    handle(const typename Transition::event_type&) const {
        return {};
    }
};

} // namespace detail

/// Variadic State: inherits a handle() for each On<Event, Target> descriptor.
///
/// Usage:
///   struct MyState : State<On<EventA, StateB>, On<EventC, StateD>> {};
template<typename... Transitions>
struct State : detail::StateHandler<Transitions>... {
    using detail::StateHandler<Transitions>::handle...;
};

} // namespace embed
