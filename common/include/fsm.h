#ifndef __FSM_H__
#define __FSM_H__

/* XXX: define STATES_LIST in a following format:
 * #define STATES_LIST(ARG, _) \
 *	_(ARG, INIT_STATE, FSM_INIT_STATE) \
 *	_(ARG, ANOTHER_STATE, other_callback) \
 *	etc... \
 *	_(ARG, LAST_STATE, FSM_LAST_STATE)
 * Arg should be passed as first arg to '_'
 * After defining this macro, include this module and call following macro:
 *	FSM(state_machine_name, STATES_LIST, userdata_t)
 *
 * Initial state should be defined via _() with 3 arguments, last argument should be FSM_INIT_STATE (see example above).
 * Last state should be defined via _() with 3 arguments, last argument should be FSM_LAST_STATE.
 * When state machine came in the last state, it will stop. Last state should not have callback
 *
 * Only one initial state and last state are expected.
 *
 * state_machine_name will be used to find state machine in current scope, objects of type userdata_t will be passed to the callback
 *
 * In function, where you want to start state machind, call
 *	FSM_RUN(name, userdata)
 *
 * name is a name of the state machine
 * userdata is an object of type userdata_t
 *
 * You should declare callbacks with followin macro:
 *	FSM_CB(name, state, userdata_arg_name)
 *
 * XXX: all callback functions will be static
 * name is a name of the state machine.
 * state is a state name, defined in the second argument in STATES_LIST macro
 * userdata_arg will be used to pass userdata into callback as (userdata_t userdata_arg_name)
 *
 * userdata can be used to catch state machine results
 *
 * FSM_STATE_TYPE macro can be used to decalre state type
 */

#define FSM_STATE_TYPE(name) __fsm_## name ##_state_cb_t
#define FSM_INIT_STATE(name) __fsm_## name ##_init_state
#define FSM_LAST_STATE(name) __fsm_## name ##_last_state

#define __FSM_LOCAL_STATE_TYPE(name) struct __fsm_## name ##_state_type_t
#define __FSM_STATE_TYPE_LOCAL(name) enum __fsm_## name ##_state_t
#define __FSM_CB_TYPE(name) __fsm_## name ##_callback_t
#define __FSM_DECLARE_STATE_NAME_IMPL(sname, name) __fsm_## sname ##_state_## name
#define __FSM_CALLBACK_NAME(name, cb_name) __fsm_## name ##_## cb_name ##_state_cb
#define __FSM_USERDATA_T_NAME(name) __fsm_## name ##_userdata_t
#define __FSM_STATES_LIST(name) __fsm_## name ##_states_list

// at least 2 args
#define __FSM_GET_MACRO(_1, _2, _3, NAME, ...) NAME
#define __FSM_DUMMY_MACRO(...)

#define __FSM_STATE(machine_name, state_name, ...) __FSM_DECLARE_STATE_NAME_IMPL(machine_name, state_name)
#define __FSM_COMMA_STATE(...) __FSM_STATE(__VA_ARGS__),

#define __FSM_FIRST_STATE(name) __FSM_STATE(name, __FSM_FIRST_STATE)
#define __FSM_MAX_ID(name) __FSM_STATE(name, __FSM_MAX_ID)

#define __FSM_DECLARE_STATES_LIST(name, STATES_LIST) \
	__FSM_STATE_TYPE_LOCAL(name) { \
		__FSM_FIRST_STATE(name) = 0, \
		STATES_LIST(name, __FSM_COMMA_STATE) \
		__FSM_MAX_ID(name) \
	};

#define __FSM_DECLARE_STATE_IMPL(sname, name, ...) [__FSM_STATE(sname, name)] = __FSM_CALLBACK_NAME(sname, name),

#define __FSM_DECLARE_STATES(name, STATES_LIST) \
	static __FSM_CB_TYPE(name) __FSM_STATES_LIST(name)[] = { \
		[__FSM_FIRST_STATE(name)] = NULL, \
		STATES_LIST(name, __FSM_DECLARE_STATE_IMPL) \
	};

#define FSM_CB(name, cb_name, userdata_arg_name) \
	static FSM_STATE_TYPE(name) __FSM_CALLBACK_NAME(name, cb_name)(__FSM_USERDATA_T_NAME(name) userdata_arg_name)

#define __FSM_PREDECLARE_FUNCTION_IMPL(name, state, ...) \
	FSM_CB(name, state, arg);

#define __FSM_DECLARE_SINGLE_STATE(name, state, ...) \
	inline static __FSM_LOCAL_STATE_TYPE(name) state() { return (__FSM_LOCAL_STATE_TYPE(name)){ .st = __FSM_STATE(name, state), }; }

#define __FSM_PREDECLARE_FUNCTIONS(name, STATES_LIST) \
	STATES_LIST(name, __FSM_PREDECLARE_FUNCTION_IMPL) \
	STATES_LIST(name, __FSM_DECLARE_SINGLE_STATE)

#define __FSM__FSM_INIT_STATE_CB_DEFINER(name, state) /* do nothing */
#define __FSM__FSM_LAST_STATE_CB_DEFINER(name, state) \
	FSM_CB(name, state, arg __attribute__((unused))) { assert(!"This function never should be called!"); return state; }

#define __FSM_MK_CB_DEFINER_FROM_STATE_TYPE(type) __FSM__## type ##_CB_DEFINER

#define __FSM_DECLARE_SERVICE_STATE(name, state, type) \
	static FSM_STATE_TYPE(name) type(name) = state; \
	__FSM_MK_CB_DEFINER_FROM_STATE_TYPE(type)(name, state)

#define __FSM_DECLARE_FIRST_AND_LAST_STATES_IMPL(...) \
	__FSM_GET_MACRO(__VA_ARGS__, __FSM_DECLARE_SERVICE_STATE, __FSM_DUMMY_MACRO)(__VA_ARGS__)

#define __FSM_DECLARE_FIRST_AND_LAST_STATES(name, STATES_LIST) \
	STATES_LIST(name, __FSM_DECLARE_FIRST_AND_LAST_STATES_IMPL)

#define FSM(name, STATES_LIST, userdata_t) \
	__FSM_DECLARE_STATES_LIST(name, STATES_LIST) \
	__FSM_LOCAL_STATE_TYPE(name) { __FSM_STATE_TYPE_LOCAL(name) st; }; \
	typedef __FSM_LOCAL_STATE_TYPE(name) (*FSM_STATE_TYPE(name))(); \
	typedef FSM_STATE_TYPE(name) (*__FSM_CB_TYPE(name))(userdata_t user_data); \
	typedef userdata_t __FSM_USERDATA_T_NAME(name); \
	__FSM_PREDECLARE_FUNCTIONS(name, STATES_LIST) \
	__FSM_DECLARE_STATES(name, STATES_LIST) \
	__FSM_DECLARE_FIRST_AND_LAST_STATES(name, STATES_LIST)

#define FSM_RUN(name, userdata) ({ \
	__FSM_STATE_TYPE_LOCAL(name) current_state = FSM_INIT_STATE(name)().st; \
	__FSM_STATE_TYPE_LOCAL(name) last_state = FSM_LAST_STATE(name)().st; \
	while (current_state != last_state) { \
		assert(current_state > __FSM_FIRST_STATE(name) && current_state < __FSM_MAX_ID(name)); \
		current_state = __FSM_STATES_LIST(name)[current_state](userdata)().st; \
	} \
})

#endif // __FSM_H__
