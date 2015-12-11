#ifndef __FSM_H__
#define __FSM_H__

/* XXX: define FSM_STATES_LIST before include this module:
 * #define FSM_STATES_LIST(ARG, _) \
 *	_(ARG, INIT_STATE, some_callback, initial_state) \
 *	_(ARG, ANOTHER_STATE, other_callback) \
 *	etc... \
 *	_(ARG, LAST_STATE)
 * Arg should be passed as first arg to '_'
 * After defining this macro, include this module and call following macro:
 *	FSM(state_machine_name, userdata_t)
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
 *	FSM_RUN(name, userdata)
 *
 * name is a name of the state machine
 * userdata is an object of type userdata_t
 *
 * You should declare callbacks with followin macro:
 *	FSM_CB(name, cb_name, userdata_arg_name)
 *
 * XXX: all callback functions will be static
 * name is a name of the state machine.
 * cb_name is a callback name
 * userdata_arg will be used to pass userdata into callback as (userdata_t userdata_arg_name)
 *
 * userdata can be used to catch state machine results
 *
 * FSM_STATE_TYPE macro can be used to decalre state type
 */

#define FSM_STATE_TYPE(name) __fsm_## name ##_state_cb_t

#define __FSM_CB_TYPE(name) __fsm_## name ##_callback_t
#define __FSM_DECLARE_STATE_NAME_IMPL(sname, name) __fsm_## sname ##_state_## name
#define __FSM_STATE_TYPE_LOCAL(name) enum __fsm_## name ##_state_t
#define __FSM_CALLBACK_NAME(name, cb_name) __fsm_## name ##_## cb_name ##_state
#define __FSM_USERDATA_T_NAME(name) __fsm_## name ##_userdata_t
#define __FSM_STATES_LIST(name) __fsm_## name ##_states_list

// at least 2 args
#define __FSM_GET_MACRO(_1, _2, _3, _4, NAME, ...) NAME
#define __FSM_DUMMY_MACRO(...)

#define __FSM_STATE(machine_name, state_name, ...) \
	__FSM_GET_MACRO(machine_name, state_name, __VA_ARGS__, __FSM_DECLARE_STATE_NAME_IMPL, __FSM_DECLARE_STATE_NAME_IMPL, __FSM_DECLARE_STATE_NAME_IMPL)(machine_name, state_name)

#define __FSM_COMMA_STATE(...) __FSM_STATE(__VA_ARGS__),

#define __FSM_DECLARE_STATES_LIST(name, STATES_LIST) \
	__FSM_STATE_TYPE_LOCAL(name) { \
		__FSM_STATE(name, FIRST) = 0, \
		STATES_LIST(name, __FSM_COMMA_STATE) \
		__FSM_STATE(name, MAX_ID) \
	};

#define __FSM_DECLARE_STATE_IMPL_3(sname, name, cb_name, ...) [__FSM_STATE(sname, name)] = __FSM_CALLBACK_NAME(sname, cb_name),
#define __FSM_DECLARE_STATE_IMPL_2(sname, name) __FSM_DECLARE_STATE_IMPL_3(sname, name, LAST_STATE)
#define __FSM_DECLARE_STATE_IMPL(...) \
	__FSM_GET_MACRO(__VA_ARGS__, __FSM_DECLARE_STATE_IMPL_3, __FSM_DECLARE_STATE_IMPL_3, __FSM_DECLARE_STATE_IMPL_2)(__VA_ARGS__)

#define __FSM_DECLARE_STATES(name, STATES_LIST) \
	static __FSM_CB_TYPE(name) __FSM_STATES_LIST(name)[] = { \
		[__FSM_STATE(name, FIRST)] = NULL, \
		STATES_LIST(name, __FSM_DECLARE_STATE_IMPL) \
	};

#define FSM_CB(name, cb_name, userdata_arg_name) \
	static FSM_STATE_TYPE(name) __FSM_CALLBACK_NAME(name, cb_name)(__FSM_USERDATA_T_NAME(name) userdata_arg_name)

#define __FSM_PREDECLARE_FUNCTION_IMPL_3(name, _, cb, ...) \
	FSM_CB(name, cb, arg);
#define __FSM_PREDECLARE_FUNCTION_IMPL_2(name, _) \
	FSM_CB(name, LAST_STATE, arg) __attribute__((noreturn));

#define __FSM_PREDECLARE_FUNCTION_IMPL(...) \
	__FSM_GET_MACRO(__VA_ARGS__, __FSM_PREDECLARE_FUNCTION_IMPL_3, __FSM_PREDECLARE_FUNCTION_IMPL_3, __FSM_PREDECLARE_FUNCTION_IMPL_2)(__VA_ARGS__)

#define __FSM_STATE_TYPE_T(name) struct __fsm_## name ##_state_type_t
#define __FSM_DECLARE_SINGLE_STATE_3(name, state, cb, ...) \
	inline static __FSM_STATE_TYPE_T(name) state() { return (__FSM_STATE_TYPE_T(name)){ .st = __FSM_STATE(name, state), }; }
#define __FSM_DECLARE_SINGLE_STATE_2(name, state) \
	__FSM_DECLARE_SINGLE_STATE_3(name, state, LAST_STATE)

#define __FSM_DECLARE_SINGLE_STATE(...) \
	__FSM_GET_MACRO(__VA_ARGS__, __FSM_DECLARE_SINGLE_STATE_3, __FSM_DECLARE_SINGLE_STATE_3, __FSM_DECLARE_SINGLE_STATE_2)(__VA_ARGS__)

#define __FSM_PREDECLARE_FUNCTIONS(name, STATES_LIST) \
	STATES_LIST(name, __FSM_PREDECLARE_FUNCTION_IMPL) \
	STATES_LIST(name, __FSM_DECLARE_SINGLE_STATE)

#define __FSM_FIRST_STATE_NAME(name) __fsm_## name ##_fists_state
#define __FSM_LAST_STATE_NAME(name) __fsm_## name ##_last_state

#define __FSM_DECLARE_FIRST_STATE(name, state, _1, _2) \
	static FSM_STATE_TYPE(name) __FSM_FIRST_STATE_NAME(name) = state;
#define __FSM_DECLARE_LAST_STATE(name, state) \
	static FSM_STATE_TYPE(name) __FSM_LAST_STATE_NAME(name) = state; \
	FSM_CB(name, LAST_STATE, arg __attribute__((unused))) { assert(!"This function never shouldn't be called!"); }

#define __FSM_DECLARE_FIRST_AND_LAST_STATES_IMPL(...) \
	__FSM_GET_MACRO(__VA_ARGS__, __FSM_DECLARE_FIRST_STATE, __FSM_DUMMY_MACRO, __FSM_DECLARE_LAST_STATE)(__VA_ARGS__)

#define __FSM_DECLARE_FIRST_AND_LAST_STATES(name, STATES_LIST) \
	STATES_LIST(name, __FSM_DECLARE_FIRST_AND_LAST_STATES_IMPL)

#define FSM(name, STATES_LIST, userdata_t) \
	__FSM_DECLARE_STATES_LIST(name, STATES_LIST) \
	typedef __FSM_STATE_TYPE_T(name) (*FSM_STATE_TYPE(name))(); \
	typedef FSM_STATE_TYPE(name) (*__FSM_CB_TYPE(name))(userdata_t user_data); \
	typedef userdata_t __FSM_USERDATA_T_NAME(name); \
	__FSM_STATE_TYPE_T(name) { __FSM_STATE_TYPE_LOCAL(name) st; }; \
	__FSM_PREDECLARE_FUNCTIONS(name, STATES_LIST) \
	__FSM_DECLARE_STATES(name, STATES_LIST) \
	__FSM_DECLARE_FIRST_AND_LAST_STATES(name, STATES_LIST)

#define FSM_RUN(name, userdata) ({ \
	__FSM_STATE_TYPE_LOCAL(name) current_state = __FSM_FIRST_STATE_NAME(name)().st; \
	__FSM_STATE_TYPE_LOCAL(name) last_state = __FSM_LAST_STATE_NAME(name)().st; \
	while (current_state != last_state) { \
		assert(current_state > __FSM_STATE(name, FIRST) && current_state < __FSM_STATE(name, MAX_ID)); \
		current_state = __FSM_STATES_LIST(name)[current_state](userdata)().st; \
	} \
})

#endif // __FSM_H__
