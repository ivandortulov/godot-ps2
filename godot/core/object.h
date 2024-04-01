/*************************************************************************/
/*  object.h                                                             */
/*************************************************************************/
/*                       This file is part of:                           */
/*                           GODOT ENGINE                                */
/*                      https://godotengine.org                          */
/*************************************************************************/
/* Copyright (c) 2007-2019 Juan Linietsky, Ariel Manzur.                 */
/* Copyright (c) 2014-2019 Godot Engine contributors (cf. AUTHORS.md)    */
/*                                                                       */
/* Permission is hereby granted, free of charge, to any person obtaining */
/* a copy of this software and associated documentation files (the       */
/* "Software"), to deal in the Software without restriction, including   */
/* without limitation the rights to use, copy, modify, merge, publish,   */
/* distribute, sublicense, and/or sell copies of the Software, and to    */
/* permit persons to whom the Software is furnished to do so, subject to */
/* the following conditions:                                             */
/*                                                                       */
/* The above copyright notice and this permission notice shall be        */
/* included in all copies or substantial portions of the Software.       */
/*                                                                       */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,       */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF    */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.*/
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY  */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,  */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE     */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                */
/*************************************************************************/
#ifndef OBJECT_H
#define OBJECT_H

#include "list.h"
#include "map.h"
#include "set.h"
#include "variant.h"
#include "vmap.h"

#define VARIANT_ARG_LIST const Variant &p_arg1 = Variant(), const Variant &p_arg2 = Variant(), const Variant &p_arg3 = Variant(), const Variant &p_arg4 = Variant(), const Variant &p_arg5 = Variant()
#define VARIANT_ARG_PASS p_arg1, p_arg2, p_arg3, p_arg4, p_arg5
#define VARIANT_ARG_DECLARE const Variant &p_arg1, const Variant &p_arg2, const Variant &p_arg3, const Variant &p_arg4, const Variant &p_arg5
#define VARIANT_ARG_MAX 5
#define VARIANT_ARGPTRS const Variant *argptr[5] = { &p_arg1, &p_arg2, &p_arg3, &p_arg4, &p_arg5 };
#define VARIANT_ARGPTRS_PASS *argptr[0], *argptr[1], *argptr[2], *argptr[3], *argptr[4]
#define VARIANT_ARGS_FROM_ARRAY(m_arr) m_arr[0], m_arr[1], m_arr[2], m_arr[3], m_arr[4]

/**
@author Juan Linietsky <reduzio@gmail.com>
*/

enum PropertyHint {
	PROPERTY_HINT_NONE, ///< no hint provided.
	PROPERTY_HINT_RANGE, ///< hint_text = "min,max,step,slider; //slider is optional"
	PROPERTY_HINT_EXP_RANGE, ///< hint_text = "min,max,step", exponential edit
	PROPERTY_HINT_ENUM, ///< hint_text= "val1,val2,val3,etc"
	PROPERTY_HINT_EXP_EASING, /// exponential easing funciton (Math::ease)
	PROPERTY_HINT_LENGTH, ///< hint_text= "length" (as integer)
	PROPERTY_HINT_SPRITE_FRAME,
	PROPERTY_HINT_KEY_ACCEL, ///< hint_text= "length" (as integer)
	PROPERTY_HINT_FLAGS, ///< hint_text= "flag1,flag2,etc" (as bit flags)
	PROPERTY_HINT_ALL_FLAGS,
	PROPERTY_HINT_FILE, ///< a file path must be passed, hint_text (optionally) is a filter "*.png,*.wav,*.doc,"
	PROPERTY_HINT_DIR, ///< a directort path must be passed
	PROPERTY_HINT_GLOBAL_FILE, ///< a file path must be passed, hint_text (optionally) is a filter "*.png,*.wav,*.doc,"
	PROPERTY_HINT_GLOBAL_DIR, ///< a directort path must be passed
	PROPERTY_HINT_RESOURCE_TYPE, ///< a resource object type
	PROPERTY_HINT_MULTILINE_TEXT, ///< used for string properties that can contain multiple lines
	PROPERTY_HINT_COLOR_NO_ALPHA, ///< used for ignoring alpha component when editing a color
	PROPERTY_HINT_IMAGE_COMPRESS_LOSSY,
	PROPERTY_HINT_IMAGE_COMPRESS_LOSSLESS,
	PROPERTY_HINT_OBJECT_ID,
	PROPERTY_HINT_MAX,
};

enum PropertyUsageFlags {

	PROPERTY_USAGE_STORAGE = 1,
	PROPERTY_USAGE_EDITOR = 2,
	PROPERTY_USAGE_NETWORK = 4,
	PROPERTY_USAGE_EDITOR_HELPER = 8,
	PROPERTY_USAGE_CHECKABLE = 16, //used for editing global variables
	PROPERTY_USAGE_CHECKED = 32, //used for editing global variables
	PROPERTY_USAGE_INTERNATIONALIZED = 64, //hint for internationalized strings
	PROPERTY_USAGE_BUNDLE = 128, //used for optimized bundles
	PROPERTY_USAGE_CATEGORY = 256,
	PROPERTY_USAGE_STORE_IF_NONZERO = 512, //only store if nonzero
	PROPERTY_USAGE_STORE_IF_NONONE = 1024, //only store if false
	PROPERTY_USAGE_NO_INSTANCE_STATE = 2048,
	PROPERTY_USAGE_RESTART_IF_CHANGED = 4096,
	PROPERTY_USAGE_SCRIPT_VARIABLE = 8192,
	PROPERTY_USAGE_STORE_IF_NULL = 16384,
	PROPERTY_USAGE_ANIMATE_AS_TRIGGER = 32768,

	PROPERTY_USAGE_SCRIPT_DEFAULT_VALUE = 1 << 17,

	PROPERTY_USAGE_DEFAULT = PROPERTY_USAGE_STORAGE | PROPERTY_USAGE_EDITOR | PROPERTY_USAGE_NETWORK,
	PROPERTY_USAGE_DEFAULT_INTL = PROPERTY_USAGE_STORAGE | PROPERTY_USAGE_EDITOR | PROPERTY_USAGE_NETWORK | PROPERTY_USAGE_INTERNATIONALIZED,
	PROPERTY_USAGE_NOEDITOR = PROPERTY_USAGE_STORAGE | PROPERTY_USAGE_NETWORK,
};

#define ADD_SIGNAL(m_signal) ObjectTypeDB::add_signal(get_type_static(), m_signal)
#define ADD_PROPERTY(m_property, m_setter, m_getter) ObjectTypeDB::add_property(get_type_static(), m_property, m_setter, m_getter)
#define ADD_PROPERTYI(m_property, m_setter, m_getter, m_index) ObjectTypeDB::add_property(get_type_static(), m_property, m_setter, m_getter, m_index)
#define ADD_PROPERTYNZ(m_property, m_setter, m_getter) ObjectTypeDB::add_property(get_type_static(), (m_property).added_usage(PROPERTY_USAGE_STORE_IF_NONZERO), m_setter, m_getter)
#define ADD_PROPERTYINZ(m_property, m_setter, m_getter, m_index) ObjectTypeDB::add_property(get_type_static(), (m_property).added_usage(PROPERTY_USAGE_STORE_IF_NONZERO), m_setter, m_getter, m_index)
#define ADD_PROPERTYNO(m_property, m_setter, m_getter) ObjectTypeDB::add_property(get_type_static(), (m_property).added_usage(PROPERTY_USAGE_STORE_IF_NONONE), m_setter, m_getter)
#define ADD_PROPERTYINO(m_property, m_setter, m_getter, m_index) ObjectTypeDB::add_property(get_type_static(), (m_property).added_usage(PROPERTY_USAGE_STORE_IF_NONONE), m_setter, m_getter, m_index)

struct PropertyInfo {

	Variant::Type type;
	String name;
	PropertyHint hint;
	String hint_string;
	uint32_t usage;

	_FORCE_INLINE_ PropertyInfo added_usage(int p_fl) const {
		PropertyInfo pi = *this;
		pi.usage |= p_fl;
		return pi;
	}

	PropertyInfo() {
		type = Variant::NIL;
		hint = PROPERTY_HINT_NONE;
		usage = PROPERTY_USAGE_DEFAULT;
	}
	PropertyInfo(Variant::Type p_type, const String p_name, PropertyHint p_hint = PROPERTY_HINT_NONE, const String &p_hint_string = "", uint32_t p_usage = PROPERTY_USAGE_DEFAULT) {
		type = p_type;
		name = p_name;
		hint = p_hint;
		hint_string = p_hint_string;
		usage = p_usage;
	}
	bool operator<(const PropertyInfo &p_info) const {
		return name < p_info.name;
	}
};

Array convert_property_list(const List<PropertyInfo> *p_list);

struct MethodInfo {

	String name;
	List<PropertyInfo> arguments;
	Vector<Variant> default_arguments;
	PropertyInfo return_val;
	uint32_t flags;
	int id;

	inline bool operator<(const MethodInfo &p_method) const { return id == p_method.id ? (name < p_method.name) : (id < p_method.id); }

	MethodInfo();
	MethodInfo(const String &p_name);
	MethodInfo(const String &p_name, const PropertyInfo &p_param1);
	MethodInfo(const String &p_name, const PropertyInfo &p_param1, const PropertyInfo &p_param2);
	MethodInfo(const String &p_name, const PropertyInfo &p_param1, const PropertyInfo &p_param2, const PropertyInfo &p_param3);
	MethodInfo(const String &p_name, const PropertyInfo &p_param1, const PropertyInfo &p_param2, const PropertyInfo &p_param3, const PropertyInfo &p_param4);
	MethodInfo(const String &p_name, const PropertyInfo &p_param1, const PropertyInfo &p_param2, const PropertyInfo &p_param3, const PropertyInfo &p_param4, const PropertyInfo &p_param5);
	MethodInfo(Variant::Type ret);
	MethodInfo(Variant::Type ret, const String &p_name);
	MethodInfo(Variant::Type ret, const String &p_name, const PropertyInfo &p_param1);
	MethodInfo(Variant::Type ret, const String &p_name, const PropertyInfo &p_param1, const PropertyInfo &p_param2);
	MethodInfo(Variant::Type ret, const String &p_name, const PropertyInfo &p_param1, const PropertyInfo &p_param2, const PropertyInfo &p_param3);
	MethodInfo(Variant::Type ret, const String &p_name, const PropertyInfo &p_param1, const PropertyInfo &p_param2, const PropertyInfo &p_param3, const PropertyInfo &p_param4);
	MethodInfo(Variant::Type ret, const String &p_name, const PropertyInfo &p_param1, const PropertyInfo &p_param2, const PropertyInfo &p_param3, const PropertyInfo &p_param4, const PropertyInfo &p_param5);
};

// old cast_to
//if ( is_type(T::get_type_static()) )
//return static_cast<T*>(this);
////else
//return NULL;

/*
   the following is an uncomprehensible blob of hacks and workarounds to compensate for many of the fallencies in C++. As a plus, this macro pretty much alone defines the object model.
*/

#define REVERSE_GET_PROPERTY_LIST                                  \
public:                                                            \
	_FORCE_INLINE_ bool _is_gpl_reversed() const { return true; }; \
                                                                   \
private:

#define UNREVERSE_GET_PROPERTY_LIST                                 \
public:                                                             \
	_FORCE_INLINE_ bool _is_gpl_reversed() const { return false; }; \
                                                                    \
private:

#define OBJ_TYPE(m_type, m_inherits)                                                                                                 \
private:                                                                                                                             \
	void operator=(const m_type &p_rval) {}                                                                                          \
	mutable StringName _type_name;                                                                                                   \
	friend class ObjectTypeDB;                                                                                                       \
                                                                                                                                     \
public:                                                                                                                              \
	virtual String get_type() const {                                                                                                \
		return String(#m_type);                                                                                                      \
	}                                                                                                                                \
	virtual const StringName *_get_type_namev() const {                                                                              \
		if (!_type_name)                                                                                                             \
			_type_name = get_type_static();                                                                                          \
		return &_type_name;                                                                                                          \
	}                                                                                                                                \
	static _FORCE_INLINE_ void *get_type_ptr_static() {                                                                              \
		static int ptr;                                                                                                              \
		return &ptr;                                                                                                                 \
	}                                                                                                                                \
	static _FORCE_INLINE_ String get_type_static() {                                                                                 \
		return String(#m_type);                                                                                                      \
	}                                                                                                                                \
	static _FORCE_INLINE_ String get_parent_type_static() {                                                                          \
		return m_inherits::get_type_static();                                                                                        \
	}                                                                                                                                \
	static void get_inheritance_list_static(List<String> *p_inheritance_list) {                                                      \
		m_inherits::get_inheritance_list_static(p_inheritance_list);                                                                 \
		p_inheritance_list->push_back(String(#m_type));                                                                              \
	}                                                                                                                                \
	static String get_category_static() {                                                                                            \
		String category = m_inherits::get_category_static();                                                                         \
		if (_get_category != m_inherits::_get_category) {                                                                            \
			if (category != "")                                                                                                      \
				category += "/";                                                                                                     \
			category += _get_category();                                                                                             \
		}                                                                                                                            \
		return category;                                                                                                             \
	}                                                                                                                                \
	static String inherits_static() {                                                                                                \
		return String(#m_inherits);                                                                                                  \
	}                                                                                                                                \
	virtual bool is_type(const String &p_type) const { return (p_type == (#m_type)) ? true : m_inherits::is_type(p_type); }          \
	virtual bool is_type_ptr(void *p_ptr) const { return (p_ptr == get_type_ptr_static()) ? true : m_inherits::is_type_ptr(p_ptr); } \
                                                                                                                                     \
	static void get_valid_parents_static(List<String> *p_parents) {                                                                  \
                                                                                                                                     \
		if (m_type::_get_valid_parents_static != m_inherits::_get_valid_parents_static) {                                            \
			m_type::_get_valid_parents_static(p_parents);                                                                            \
		}                                                                                                                            \
                                                                                                                                     \
		m_inherits::get_valid_parents_static(p_parents);                                                                             \
	}                                                                                                                                \
                                                                                                                                     \
protected:                                                                                                                           \
	_FORCE_INLINE_ static void (*_get_bind_methods())() {                                                                            \
		return &m_type::_bind_methods;                                                                                               \
	}                                                                                                                                \
                                                                                                                                     \
public:                                                                                                                              \
	static void initialize_type() {                                                                                                  \
		static bool initialized = false;                                                                                             \
		if (initialized)                                                                                                             \
			return;                                                                                                                  \
		m_inherits::initialize_type();                                                                                               \
		ObjectTypeDB::_add_type<m_type>();                                                                                           \
		if (m_type::_get_bind_methods() != m_inherits::_get_bind_methods())                                                          \
			_bind_methods();                                                                                                         \
		initialized = true;                                                                                                          \
	}                                                                                                                                \
                                                                                                                                     \
protected:                                                                                                                           \
	virtual void _initialize_typev() {                                                                                               \
		initialize_type();                                                                                                           \
	}                                                                                                                                \
	_FORCE_INLINE_ bool (Object::*(_get_get() const))(const StringName &p_name, Variant &) const {                                   \
		return (bool (Object::*)(const StringName &, Variant &) const) & m_type::_get;                                               \
	}                                                                                                                                \
	virtual bool _getv(const StringName &p_name, Variant &r_ret) const {                                                             \
		if (m_type::_get_get() != m_inherits::_get_get()) {                                                                          \
			if (_get(p_name, r_ret))                                                                                                 \
				return true;                                                                                                         \
		}                                                                                                                            \
		return m_inherits::_getv(p_name, r_ret);                                                                                     \
	}                                                                                                                                \
	_FORCE_INLINE_ bool (Object::*(_get_set() const))(const StringName &p_name, const Variant &p_property) {                         \
		return (bool (Object::*)(const StringName &, const Variant &)) & m_type::_set;                                               \
	}                                                                                                                                \
	virtual bool _setv(const StringName &p_name, const Variant &p_property) {                                                        \
		if (m_inherits::_setv(p_name, p_property)) return true;                                                                      \
		if (m_type::_get_set() != m_inherits::_get_set()) {                                                                          \
			return _set(p_name, p_property);                                                                                         \
		}                                                                                                                            \
		return false;                                                                                                                \
	}                                                                                                                                \
	_FORCE_INLINE_ void (Object::*(_get_get_property_list() const))(List<PropertyInfo> * p_list) const {                             \
		return (void (Object::*)(List<PropertyInfo> *) const) & m_type::_get_property_list;                                          \
	}                                                                                                                                \
	virtual void _get_property_listv(List<PropertyInfo> *p_list, bool p_reversed) const {                                            \
		if (!p_reversed) {                                                                                                           \
			m_inherits::_get_property_listv(p_list, p_reversed);                                                                     \
		}                                                                                                                            \
		p_list->push_back(PropertyInfo(Variant::NIL, get_type_static(), PROPERTY_HINT_NONE, String(), PROPERTY_USAGE_CATEGORY));     \
		if (!_is_gpl_reversed())                                                                                                     \
			ObjectTypeDB::get_property_list(#m_type, p_list, true, this);                                                            \
		if (m_type::_get_get_property_list() != m_inherits::_get_get_property_list()) {                                              \
			_get_property_list(p_list);                                                                                              \
		}                                                                                                                            \
		if (_is_gpl_reversed())                                                                                                      \
			ObjectTypeDB::get_property_list(#m_type, p_list, true, this);                                                            \
		if (p_reversed) {                                                                                                            \
			m_inherits::_get_property_listv(p_list, p_reversed);                                                                     \
		}                                                                                                                            \
	}                                                                                                                                \
	_FORCE_INLINE_ void (Object::*(_get_notification() const))(int) {                                                                \
		return (void (Object::*)(int)) & m_type::_notification;                                                                      \
	}                                                                                                                                \
	virtual void _notificationv(int p_notification, bool p_reversed) {                                                               \
		if (!p_reversed)                                                                                                             \
			m_inherits::_notificationv(p_notification, p_reversed);                                                                  \
		if (m_type::_get_notification() != m_inherits::_get_notification()) {                                                        \
			_notification(p_notification);                                                                                           \
		}                                                                                                                            \
		if (p_reversed)                                                                                                              \
			m_inherits::_notificationv(p_notification, p_reversed);                                                                  \
	}                                                                                                                                \
                                                                                                                                     \
private:

#define OBJ_CATEGORY(m_category)                                        \
protected:                                                              \
	_FORCE_INLINE_ static String _get_category() { return m_category; } \
                                                                        \
private:

#define OBJ_SAVE_TYPE(m_type)                                \
public:                                                      \
	virtual String get_save_type() const { return #m_type; } \
                                                             \
private:

class ScriptInstance;
typedef uint32_t ObjectID;

class Object {
public:
	enum ConnectFlags {

		CONNECT_DEFERRED = 1,
		CONNECT_PERSIST = 2, // hint for scene to save this connection
		CONNECT_ONESHOT = 4
	};

	struct Connection {

		Object *source;
		StringName signal;
		Object *target;
		StringName method;
		uint32_t flags;
		Vector<Variant> binds;
		bool operator<(const Connection &p_conn) const;

		operator Variant() const;
		Connection() {
			source = NULL;
			target = NULL;
			flags = 0;
		}
		Connection(const Variant &p_variant);
	};

private:
#ifdef DEBUG_ENABLED
	friend class _ObjectDebugLock;
#endif
	friend bool predelete_handler(Object *);
	friend void postinitialize_handler(Object *);

	struct Signal {

		struct Target {

			ObjectID _id;
			StringName method;

			_FORCE_INLINE_ bool operator<(const Target &p_target) const { return (_id == p_target._id) ? (method < p_target.method) : (_id < p_target._id); }

			Target(const ObjectID &p_id, const StringName &p_method) {
				_id = p_id;
				method = p_method;
			}
			Target() { _id = 0; }
		};

		struct Slot {

			Connection conn;
			List<Connection>::Element *cE;
		};

		MethodInfo user;
		VMap<Target, Slot> slot_map;
		int lock;
		Signal() { lock = 0; }
	};

	HashMap<StringName, Signal, StringNameHasher> signal_map;
	List<Connection> connections;
#ifdef DEBUG_ENABLED
	SafeRefCount _lock_index;
#endif
	bool _block_signals;
	int _predelete_ok;
	Set<Object *> change_receptors;
	uint32_t _instance_ID;
	bool _predelete();
	void _postinitialize();
	bool _can_translate;
#ifdef TOOLS_ENABLED
	bool _edited;
	uint32_t _edited_version;
#endif
	ScriptInstance *script_instance;
	RefPtr script;
	Dictionary metadata;
	mutable StringName _type_name;
	mutable const StringName *_type_ptr;

	void _add_user_signal(const String &p_name, const Array &p_pargs = Array());
	bool _has_user_signal(const StringName &p_name) const;
	Variant _emit_signal(const Variant **p_args, int p_argcount, Variant::CallError &r_error);
	Array _get_signal_list() const;
	Array _get_signal_connection_list(const String &p_signal) const;
	void _set_bind(const String &p_set, const Variant &p_value);
	Variant _get_bind(const String &p_name) const;

	void property_list_changed_notify();

protected:
	virtual bool _use_builtin_script() const { return false; }
	virtual void _initialize_typev() { initialize_type(); }
	virtual bool _setv(const StringName &p_name, const Variant &p_property) { return false; };
	virtual bool _getv(const StringName &p_name, Variant &r_property) const { return false; };
	virtual void _get_property_listv(List<PropertyInfo> *p_list, bool p_reversed) const {};
	virtual void _notificationv(int p_notification, bool p_reversed){};

	static String _get_category() { return ""; }
	static void _bind_methods();
	bool _set(const StringName &p_name, const Variant &p_property) { return false; };
	bool _get(const StringName &p_name, Variant &r_property) const { return false; };
	void _get_property_list(List<PropertyInfo> *p_list) const {};
	void _notification(int p_notification){};

	_FORCE_INLINE_ static void (*_get_bind_methods())() {
		return &Object::_bind_methods;
	}
	_FORCE_INLINE_ bool (Object::*(_get_get() const))(const StringName &p_name, Variant &r_ret) const {
		return &Object::_get;
	}
	_FORCE_INLINE_ bool (Object::*(_get_set() const))(const StringName &p_name, const Variant &p_property) {
		return &Object::_set;
	}
	_FORCE_INLINE_ void (Object::*(_get_get_property_list() const))(List<PropertyInfo> *p_list) const {
		return &Object::_get_property_list;
	}
	_FORCE_INLINE_ void (Object::*(_get_notification() const))(int) {
		return &Object::_notification;
	}
	static void get_valid_parents_static(List<String> *p_parents);
	static void _get_valid_parents_static(List<String> *p_parents);

	void cancel_delete();

	virtual void _changed_callback(Object *p_changed, const char *p_prop);

	//Variant _call_bind(const StringName& p_name, const Variant& p_arg1 = Variant(), const Variant& p_arg2 = Variant(), const Variant& p_arg3 = Variant(), const Variant& p_arg4 = Variant());
	//void _call_deferred_bind(const StringName& p_name, const Variant& p_arg1 = Variant(), const Variant& p_arg2 = Variant(), const Variant& p_arg3 = Variant(), const Variant& p_arg4 = Variant());

	Variant _call_bind(const Variant **p_args, int p_argcount, Variant::CallError &r_error);
	Variant _call_deferred_bind(const Variant **p_args, int p_argcount, Variant::CallError &r_error);

	virtual const StringName *_get_type_namev() const {
		if (!_type_name)
			_type_name = get_type_static();
		return &_type_name;
	}

	DVector<String> _get_meta_list_bind() const;
	Array _get_property_list_bind() const;
	Array _get_method_list_bind() const;

	void _clear_internal_resource_paths(const Variant &p_var);

	friend class ObjectTypeDB;
	virtual void _validate_property(PropertyInfo &property) const;

public: //should be protected, but bug in clang++
	static void initialize_type();
	_FORCE_INLINE_ static void register_custom_data_to_otdb(){};

public:
#ifdef TOOLS_ENABLED
	_FORCE_INLINE_ void _change_notify(const char *p_property = "") {
		_edited = true;
		for (Set<Object *>::Element *E = change_receptors.front(); E; E = E->next())
			((Object *)(E->get()))->_changed_callback(this, p_property);
	}
#else
	_FORCE_INLINE_ void _change_notify(const char *p_what = "") {}
#endif
	static void *get_type_ptr_static() {
		static int ptr;
		return &ptr;
	}

	bool _is_gpl_reversed() const { return false; }

	_FORCE_INLINE_ ObjectID get_instance_ID() const { return _instance_ID; }
	// this is used for editors

	void add_change_receptor(Object *p_receptor);
	void remove_change_receptor(Object *p_receptor);

// TODO: ensure 'this' is never NULL since it's UB, but by now, avoid warning flood
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wundefined-bool-conversion"
#endif

	template <class T>
	T *cast_to() {

#ifndef NO_SAFE_CAST
		return SAFE_CAST<T *>(this);
#else
		if (!this)
			return NULL;
		if (is_type_ptr(T::get_type_ptr_static()))
			return static_cast<T *>(this);
		else
			return NULL;
#endif
	}

	template <class T>
	const T *cast_to() const {

#ifndef NO_SAFE_CAST
		return SAFE_CAST<const T *>(this);
#else
		if (!this)
			return NULL;
		if (is_type_ptr(T::get_type_ptr_static()))
			return static_cast<const T *>(this);
		else
			return NULL;
#endif
	}

#ifdef __clang__
#pragma clang diagnostic pop
#endif

	enum {

		NOTIFICATION_POSTINITIALIZE = 0,
		NOTIFICATION_PREDELETE = 1
	};

	/* TYPE API */
	static void get_inheritance_list_static(List<String> *p_inheritance_list) { p_inheritance_list->push_back("Object"); }

	static String get_type_static() { return "Object"; }
	static String get_parent_type_static() { return String(); }
	static String get_category_static() { return String(); }

	virtual String get_type() const { return "Object"; }
	virtual String get_save_type() const { return get_type(); } //type stored when saving

	virtual bool is_type(const String &p_type) const { return (p_type == "Object"); }
	virtual bool is_type_ptr(void *p_ptr) const { return get_type_ptr_static() == p_ptr; }

	_FORCE_INLINE_ const StringName &get_type_name() const {
		if (!_type_ptr) {
			return *_get_type_namev();
		} else {
			return *_type_ptr;
		}
	}

	/* IAPI */
	//	void set(const String& p_name, const Variant& p_value);
	//	Variant get(const String& p_name) const;

	void set(const StringName &p_name, const Variant &p_value, bool *r_valid = NULL);
	Variant get(const StringName &p_name, bool *r_valid = NULL) const;

	void get_property_list(List<PropertyInfo> *p_list, bool p_reversed = false) const;

	bool has_method(const StringName &p_method) const;
	void get_method_list(List<MethodInfo> *p_list) const;
	Variant callv(const StringName &p_method, const Array &p_args);
	virtual Variant call(const StringName &p_method, const Variant **p_args, int p_argcount, Variant::CallError &r_error);
	virtual void call_multilevel(const StringName &p_method, const Variant **p_args, int p_argcount);
	virtual void call_multilevel_reversed(const StringName &p_method, const Variant **p_args, int p_argcount);
	Variant call(const StringName &p_name, VARIANT_ARG_LIST); // C++ helper
	void call_multilevel(const StringName &p_name, VARIANT_ARG_LIST); // C++ helper

	void notification(int p_notification, bool p_reversed = false);

	//used mainly by script, get and set all INCLUDING string
	virtual Variant getvar(const Variant &p_key, bool *r_valid = NULL) const;
	virtual void setvar(const Variant &p_key, const Variant &p_value, bool *r_valid = NULL);

	/* SCRIPT */

	void set_script(const RefPtr &p_script);
	RefPtr get_script() const;

	/* SCRIPT */

	bool has_meta(const String &p_name) const;
	void set_meta(const String &p_name, const Variant &p_value);
	Variant get_meta(const String &p_name) const;
	void get_meta_list(List<String> *p_list) const;

#ifdef TOOLS_ENABLED
	void set_edited(bool p_edited);
	bool is_edited() const;
	uint32_t get_edited_version() const; //this function is used to check when something changed beyond a point, it's used mainly for generating previews
#endif

	void set_script_instance(ScriptInstance *p_instance);
	_FORCE_INLINE_ ScriptInstance *get_script_instance() const { return script_instance; }

	void add_user_signal(const MethodInfo &p_signal);
	void emit_signal(const StringName &p_name, VARIANT_ARG_LIST);
	void emit_signal(const StringName &p_name, const Variant **p_args, int p_argcount);
	void get_signal_list(List<MethodInfo> *p_signals) const;
	void get_signal_connection_list(const StringName &p_signal, List<Connection> *p_connections) const;
	void get_all_signal_connections(List<Connection> *p_connections) const;
	bool has_persistent_signal_connections() const;
	void get_signals_connected_to_this(List<Connection> *p_connections) const;

	Error connect(const StringName &p_signal, Object *p_to_object, const StringName &p_to_method, const Vector<Variant> &p_binds = Vector<Variant>(), uint32_t p_flags = 0);
	void disconnect(const StringName &p_signal, Object *p_to_object, const StringName &p_to_method);
	bool is_connected(const StringName &p_signal, Object *p_to_object, const StringName &p_to_method) const;

	void call_deferred(const StringName &p_method, VARIANT_ARG_LIST);

	void set_block_signals(bool p_block);
	bool is_blocking_signals() const;

	Variant::Type get_static_property_type(const StringName &p_property, bool *r_valid = NULL) const;

	virtual void get_translatable_strings(List<String> *p_strings) const;

	virtual void get_argument_options(const StringName &p_function, int p_idx, List<String> *r_options) const;

	StringName XL_MESSAGE(const StringName &p_message) const; //translate message (internationalization)
	StringName tr(const StringName &p_message) const; //translate message (alternative)

	bool _is_queued_for_deletion; // set to true by SceneTree::queue_delete()
	bool is_queued_for_deletion() const;

	_FORCE_INLINE_ void set_message_translation(bool p_enable) { _can_translate = p_enable; }
	_FORCE_INLINE_ bool can_translate_messages() const { return _can_translate; }

	void clear_internal_resource_paths();

	Object();
	virtual ~Object();
};

bool predelete_handler(Object *p_object);
void postinitialize_handler(Object *p_object);

class ObjectDB {

	struct ObjectPtrHash {

		static _FORCE_INLINE_ uint32_t hash(const Object *p_obj) {

			union {
				const Object *p;
				unsigned long i;
			} u;
			u.p = p_obj;
			return HashMapHasherDefault::hash((uint64_t)u.i);
		}
	};

	static HashMap<uint32_t, Object *> instances;
	static HashMap<Object *, ObjectID, ObjectPtrHash> instance_checks;

	static uint32_t instance_counter;
	friend class Object;
	friend void unregister_core_types();

	static void cleanup();
	static uint32_t add_instance(Object *p_object);
	static void remove_instance(Object *p_object);

public:
	typedef void (*DebugFunc)(Object *p_obj);

	static Object *get_instance(uint32_t p_instance_ID);
	static void debug_objects(DebugFunc p_func);
	static int get_object_count();

#ifdef DEBUG_ENABLED
	_FORCE_INLINE_ static bool instance_validate(Object *p_ptr) {

		return instance_checks.has(p_ptr);
	}
#else
	_FORCE_INLINE_ static bool instance_validate(Object *p_ptr) { return true; }

#endif
};

//needed by macros
#include "object_type_db.h"

#endif
