#ifndef __STATE_MACHINE_H__
#define __STATE_MACHINE_H__

/* XXX: define STATE_MACHINE_STATES_LIST before include this module:
 * #define STATE_MACHINE_STATES_LIST(ARG, _) \
 *	_(ARG, INIT_STATE, some_callback, initial_state) \
 *	_(ARG, ANOTHER_STATE, other_callback) \
 *	etc... \
 *	_(ARG, LAST_STATE)
 * Arg should be passed as first arg to '_'
 * After defining this macro, include this module and call following macro:
 *	STATE_MACHINE(state_machine_name, userdata_t)
 *
 * Initial state should be defined via _() with 4 arguments (see example above).
 * Forth argument will be ignored and will be used just a flag.
 *
 * Last state should be defined via _() with 2 arguments (without callback).
 * When state machine came in the last state, it will stop.
 *
 * Only one initial state and last state are expected.
 *
 * state_machine_t will be used to find state machine in current scope, objects of type userdata_t will be passed to the callback
 *
 * In function, where you want to start state machind, call
 *	STATE_MACHINE_RUN(name, userdata)
 *
 * name is a name of the state machine
 * userdata is an object of type userdata_t
 *
 * You should declare callbacks with followin macro:
 *	STATE_MACHINE_CB(name, cb_name, userdata_arg_name)
 *
 * XXX: all callback functions will be static
 * name is a name of the state machine.
 * cb_name is a callback name
 * userdata_arg will be used to pass userdata into callback as (userdata_t userdata_arg_name)
 *
 * userdata can be used to catch state machine results
 *
 * STATE_MACHINE_STATE_TYPE macro can be used to decalre state type
 */

#define STATE_MACHINE_STATE_TYPE(name) __state_machine_## name ##_state_cb_t

#define __STATE_MACHINE_CB_TYPE(name) __state_machine_## name ##_callback_t
#define __DECLARE_STATE_NAME_IMPL(sname, name) __state_machine_## sname ##_state_## name
#define __STATE_TYPE_LOCAL(name) enum __state_machine_## name ##_state_t
#define __CALLBACK_NAME(name, cb_name) __state_machine_## name ##_## cb_name ##_state
#define __USERDATA_T_NAME(name) __state_machine_## name ##_userdata_t

// at least 2 args
#define __GET_MACRO(_1, _2, _3, _4, NAME, ...) NAME
#define __DUMMY_MACRO(...)

#define __STATE(machine_name, state_name, ...) \
	__GET_MACRO(machine_name, state_name, __VA_ARGS__, __DECLARE_STATE_NAME_IMPL, __DECLARE_STATE_NAME_IMPL, __DECLARE_STATE_NAME_IMPL)(machine_name, state_name)

#define __COMMA_STATE(...) __STATE(__VA_ARGS__),

#define __DECLARE_STATES_LIST(name, STATES_LIST) \
	__STATE_TYPE_LOCAL(name) { \
		__STATE(name, FIRST) = 0, \
		STATES_LIST(name, __COMMA_STATE) \
		__STATE(name, MAX_ID) \
	};

#define __DECLARE_STATE_IMPL_3(sname, name, cb_name, ...) [__STATE(sname, name)] = __CALLBACK_NAME(sname, cb_name),
#define __DECLARE_STATE_IMPL_2(sname, name) __DECLARE_STATE_IMPL_3(sname, name, LAST_STATE)
#define __DECLARE_STATE_IMPL(...) \
	__GET_MACRO(__VA_ARGS__, __DECLARE_STATE_IMPL_3, __DECLARE_STATE_IMPL_3, __DECLARE_STATE_IMPL_2)(__VA_ARGS__)

#define __DECLARE_STATES(name, STATES_LIST) \
	static __STATE_MACHINE_CB_TYPE(name) __state_machine_## name ##_states_list[] = { \
		[__STATE(name, FIRST)] = NULL, \
		STATES_LIST(name, __DECLARE_STATE_IMPL) \
	};

#define STATE_MACHINE_CB(name, cb_name, userdata_arg_name) \
	static STATE_MACHINE_STATE_TYPE(name) __CALLBACK_NAME(name, cb_name)(__USERDATA_T_NAME(name) userdata_arg_name)

#define __PREDECLARE_FUNCTION_IMPL_3(name, _, cb, ...) \
	STATE_MACHINE_CB(name, cb, arg);
#define __PREDECLARE_FUNCTION_IMPL_2(name, _) \
	STATE_MACHINE_CB(name, LAST_STATE, arg) __attribute__((noreturn));

#define __PREDECLARE_FUNCTION_IMPL(...) \
	__GET_MACRO(__VA_ARGS__, __PREDECLARE_FUNCTION_IMPL_3, __PREDECLARE_FUNCTION_IMPL_3, __PREDECLARE_FUNCTION_IMPL_2)(__VA_ARGS__)

#define __STATE_TYPE_T(name) struct __state_machine_## name ##_state_type_t
#define __DECLARE_SINGLE_STATE_3(name, state, cb, ...) \
	inline static __STATE_TYPE_T(name) state() { return (__STATE_TYPE_T(name)){ .st = __STATE(name, state), }; }
#define __DECLARE_SINGLE_STATE_2(name, state) \
	__DECLARE_SINGLE_STATE_3(name, state, LAST_STATE)

#define __DECLARE_SINGLE_STATE(...) \
	__GET_MACRO(__VA_ARGS__, __DECLARE_SINGLE_STATE_3, __DECLARE_SINGLE_STATE_3, __DECLARE_SINGLE_STATE_2)(__VA_ARGS__)

#define __PREDECLARE_FUNCTIONS(name, STATES_LIST) \
	STATES_LIST(name, __PREDECLARE_FUNCTION_IMPL) \
	STATES_LIST(name, __DECLARE_SINGLE_STATE)

#define __STATE_MACHINE_FIRST_STATE_NAME(name) __state_machine_## name ##_fists_state
#define __STATE_MACHINE_LAST_STATE_NAME(name) __state_machine_## name ##_last_state

#define __DECLARE_FIRST_STATE(name, state, _1, _2) \
	static STATE_MACHINE_STATE_TYPE(name) __STATE_MACHINE_FIRST_STATE_NAME(name) = state;
#define __DECLARE_LAST_STATE(name, state) \
	static STATE_MACHINE_STATE_TYPE(name) __STATE_MACHINE_LAST_STATE_NAME(name) = state; \
	STATE_MACHINE_CB(name, LAST_STATE, arg __attribute__((unused))) { assert(!"This function never shouldn't be called!"); }

#define __DECLARE_FIRST_AND_LAST_STATES_IMPL(...) \
	__GET_MACRO(__VA_ARGS__, __DECLARE_FIRST_STATE, __DUMMY_MACRO, __DECLARE_LAST_STATE)(__VA_ARGS__)

#define __DECLARE_FIRST_AND_LAST_STATES(name, STATES_LIST) \
	STATES_LIST(name, __DECLARE_FIRST_AND_LAST_STATES_IMPL)

#define STATE_MACHINE(name, STATES_LIST, userdata_t) \
	__DECLARE_STATES_LIST(name, STATES_LIST) \
	typedef __STATE_TYPE_T(name) (*STATE_MACHINE_STATE_TYPE(name))(); \
	typedef STATE_MACHINE_STATE_TYPE(name) (*__STATE_MACHINE_CB_TYPE(name))(userdata_t user_data); \
	typedef userdata_t __USERDATA_T_NAME(name); \
	__STATE_TYPE_T(name) { __STATE_TYPE_LOCAL(name) st; }; \
	__PREDECLARE_FUNCTIONS(name, STATES_LIST) \
	__DECLARE_STATES(name, STATES_LIST) \
	__DECLARE_FIRST_AND_LAST_STATES(name, STATES_LIST)

#define STATE_MACHINE_RUN(name, userdata) ({ \
	__STATE_TYPE_LOCAL(name) current_state = __STATE_MACHINE_FIRST_STATE_NAME(name)().st; \
	__STATE_TYPE_LOCAL(name) last_state = __STATE_MACHINE_LAST_STATE_NAME(name)().st; \
	while (current_state != last_state) { \
		assert(current_state > __STATE(name, FIRST) && current_state < __STATE(name, MAX_ID)); \
		current_state = __state_machine_## name ##_states_list[current_state](userdata)().st; \
	} \
})

#endif // __STATE_MACHINE_H__
