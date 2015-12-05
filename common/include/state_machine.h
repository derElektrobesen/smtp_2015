#ifndef __STATE_MACHINE_H__
#define __STATE_MACHINE_H__

/* XXX: define STATE_MACHINE_STATES_LIST before include this module:
 * #define STATE_MACHINE_STATES_LIST \
 *	STATE(INIT_STATE, some_callback) \
 *	STATE(ANOTHER_STATE, other_callback) \
 *	etc...
 *
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

#define STATE_MACHINE_STATE_TYPE(name) enum __state_machine_## name ##_state_t
#define STATE_MACHINE_CB_TYPE(name) __state_machine_## name ##_callback_t

#define __DECLARE_STATE_NAME_IMPL(sname, name, cb) __state_machine_## sname ##_state_## name

#define STATE(machine_name, state_name) __DECLARE_STATE_NAME_IMPL(machine_name, state_name, _)
#define __COMMA_STATE(sname, name, _) STATE(sname, name),

#define __DECLARE_STATES_TYPES(name, STATES_LIST) \
	STATE_MACHINE_STATE_TYPE(name) { \
		STATE(name, FIRST) = 0, \
		STATES_LIST(name, __COMMA_STATE) \
		STATE(name, MAX_ID) \
	};

#define __DECLARE_STATE_IMPL(sname, name, cb_name) [STATE(sname, name)] = __state_machine_## cb_name ##_state,

#define __DECLARE_STATES(name, STATES_LIST) \
	static STATE_MACHINE_CB_TYPE(name) __state_machine_## name ##_states_list[] = { \
		[STATE(name, FIRST)] = NULL, \
		STATES_LIST(name, __DECLARE_STATE_IMPL) \
	};

#define STATE_MACHINE_CB(name, cb_name, userdata_arg_name) \
	static STATE_MACHINE_STATE_TYPE(name) __state_machine_## cb_name ##_state(__state_machine_## name ##_userdata_t userdata_arg_name)

#define __PREDECLARE_FUNCTION_IMPL(name, _, cb) \
	STATE_MACHINE_CB(name, cb, arg);

#define __PREDECLARE_FUNCTIONS(name, STATES_LIST) \
	STATES_LIST(name, __PREDECLARE_FUNCTION_IMPL)

#define STATE_MACHINE(name, STATES_LIST, userdata_t) \
	__DECLARE_STATES_TYPES(name, STATES_LIST) \
	typedef STATE_MACHINE_STATE_TYPE(name) (*STATE_MACHINE_CB_TYPE(name))(userdata_t user_data); \
	typedef userdata_t __state_machine_## name ##_userdata_t; \
	__PREDECLARE_FUNCTIONS(name, STATES_LIST) \
	__DECLARE_STATES(name, STATES_LIST)

#define STATE_MACHINE_RUN(name, userdata, initial_state, finish_state) ({ \
	STATE_MACHINE_STATE_TYPE(name) current_state = STATE(name, initial_state); \
	while (current_state != STATE(name, finish_state)) { \
		assert(current_state > STATE(name, FIRST) && current_state < STATE(name, MAX_ID)); \
		STATE_MACHINE_STATE_TYPE(name) new_state = __state_machine_## name ##_states_list[current_state](userdata); \
		current_state = new_state; \
	} \
})

#endif // __STATE_MACHINE_H__
