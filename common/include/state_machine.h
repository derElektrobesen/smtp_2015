#ifndef __STATE_MACHINE_H__
#define __STATE_MACHINE_H__

/* XXX: define STATE_MACHINE_STATES_LIST before include this module:
 * #define STATE_MACHINE_STATES_LIST(ARG, _) \
 *	_(ARG, INIT_STATE, some_callback) \
 *	_(ARG, ANOTHER_STATE, other_callback) \
 *	etc...
 * Arg should be passed as first arg to '_'
 * After defining this macro, include this module and call following macro:
 *	STATE_MACHINE(state_machine_name, userdata_t)
 *
 * state_machine_t will be used to find state machine in current scope, objects of type userdata_t will be passed to the callback
 *
 * In function, where you want to start state machind, call
 *	STATE_MACHINE_RUN(name, userdata, initial_state, finish_state)
 *
 * name is a name of the state machine
 * userdata is an object of type userdata_t
 * initial state will be passes as initial
 * when state machine came in finish_state it will stop.
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
#define STATE_MACHINE_CB_TYPE(name) __state_machine_## name ##_callback_t

#define __DECLARE_STATE_NAME_IMPL(sname, name, cb) __state_machine_## sname ##_state_## name
#define __STATE_TYPE_LOCAL(name) enum __state_machine_## name ##_state_t

#define __STATE(machine_name, state_name) __DECLARE_STATE_NAME_IMPL(machine_name, state_name, _)
#define __COMMA_STATE(sname, name, _) __STATE(sname, name),

#define __DECLARE_STATES_LIST(name, STATES_LIST) \
	__STATE_TYPE_LOCAL(name) { \
		__STATE(name, FIRST) = 0, \
		STATES_LIST(name, __COMMA_STATE) \
		__STATE(name, MAX_ID) \
	};

#define __DECLARE_STATE_IMPL(sname, name, cb_name) [__STATE(sname, name)] = __state_machine_## cb_name ##_state,

#define __DECLARE_STATES(name, STATES_LIST) \
	static STATE_MACHINE_CB_TYPE(name) __state_machine_## name ##_states_list[] = { \
		[__STATE(name, FIRST)] = NULL, \
		STATES_LIST(name, __DECLARE_STATE_IMPL) \
	};

#define STATE_MACHINE_CB(name, cb_name, userdata_arg_name) \
	static STATE_MACHINE_STATE_TYPE(name) __state_machine_## cb_name ##_state(__state_machine_## name ##_userdata_t userdata_arg_name)

#define __PREDECLARE_FUNCTION_IMPL(name, _, cb) \
	STATE_MACHINE_CB(name, cb, arg);

#define __STATE_TYPE_T(name) struct __state_machine_## name ##_state_type_t
#define __DECLARE_SINGLE_STATE(name, state, cb) \
	inline static __STATE_TYPE_T(name) state() { return (__STATE_TYPE_T(name)){ .st = __STATE(name, state), }; }

#define __PREDECLARE_FUNCTIONS(name, STATES_LIST) \
	STATES_LIST(name, __PREDECLARE_FUNCTION_IMPL) \
	STATES_LIST(name, __DECLARE_SINGLE_STATE)

#define STATE_MACHINE(name, STATES_LIST, userdata_t) \
	__DECLARE_STATES_LIST(name, STATES_LIST) \
	typedef __STATE_TYPE_T(name) (*STATE_MACHINE_STATE_TYPE(name))(); \
	typedef STATE_MACHINE_STATE_TYPE(name) (*STATE_MACHINE_CB_TYPE(name))(userdata_t user_data); \
	typedef userdata_t __state_machine_## name ##_userdata_t; \
	__STATE_TYPE_T(name) { __STATE_TYPE_LOCAL(name) st; }; \
	__PREDECLARE_FUNCTIONS(name, STATES_LIST) \
	__DECLARE_STATES(name, STATES_LIST)

#define STATE_MACHINE_RUN(name, userdata, initial_state, __finish_state) ({ \
	__STATE_TYPE_LOCAL(name) current_state = initial_state().st; \
	__STATE_TYPE_LOCAL(name) finish_state = __finish_state().st; \
	while (current_state != finish_state) { \
		assert(current_state > __STATE(name, FIRST) && current_state < __STATE(name, MAX_ID)); \
		current_state = __state_machine_## name ##_states_list[current_state](userdata)().st; \
	} \
})

#endif // __STATE_MACHINE_H__
