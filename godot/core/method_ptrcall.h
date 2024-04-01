#ifndef METHOD_PTRCALL_H
#define METHOD_PTRCALL_H

#include "math_2d.h"
#include "typedefs.h"
#include "variant.h"

#ifdef PTRCALL_ENABLED

template <class T>
struct PtrToArg {
};

#define MAKE_PTRARG(m_type)                                            \
	template <>                                                        \
	struct PtrToArg<m_type> {                                          \
		_FORCE_INLINE_ static m_type convert(const void *p_ptr) {      \
			return *reinterpret_cast<const m_type *>(p_ptr);           \
		}                                                              \
		_FORCE_INLINE_ static void encode(m_type p_val, void *p_ptr) { \
			*((m_type *)p_ptr) = p_val;                                \
		}                                                              \
	};                                                                 \
	template <>                                                        \
	struct PtrToArg<const m_type &> {                                  \
		_FORCE_INLINE_ static m_type convert(const void *p_ptr) {      \
			return *reinterpret_cast<const m_type *>(p_ptr);           \
		}                                                              \
		_FORCE_INLINE_ static void encode(m_type p_val, void *p_ptr) { \
			*((m_type *)p_ptr) = p_val;                                \
		}                                                              \
	}

#define MAKE_PTRARGR(m_type, m_ret)                                    \
	template <>                                                        \
	struct PtrToArg<m_type> {                                          \
		_FORCE_INLINE_ static m_type convert(const void *p_ptr) {      \
			return *reinterpret_cast<const m_type *>(p_ptr);           \
		}                                                              \
		_FORCE_INLINE_ static void encode(m_type p_val, void *p_ptr) { \
			*((m_ret *)p_ptr) = p_val;                                 \
		}                                                              \
	};                                                                 \
	template <>                                                        \
	struct PtrToArg<const m_type &> {                                  \
		_FORCE_INLINE_ static m_type convert(const void *p_ptr) {      \
			return *reinterpret_cast<const m_type *>(p_ptr);           \
		}                                                              \
		_FORCE_INLINE_ static void encode(m_type p_val, void *p_ptr) { \
			*((m_ret *)p_ptr) = p_val;                                 \
		}                                                              \
	}

MAKE_PTRARG(bool);
MAKE_PTRARGR(uint8_t, int);
MAKE_PTRARGR(int8_t, int);
MAKE_PTRARGR(uint16_t, int);
MAKE_PTRARGR(int16_t, int);
MAKE_PTRARGR(uint32_t, int);
MAKE_PTRARGR(int32_t, int);
MAKE_PTRARGR(int64_t, int);
MAKE_PTRARGR(uint64_t, int);
MAKE_PTRARG(float);
MAKE_PTRARGR(double, float);

MAKE_PTRARG(String);
MAKE_PTRARG(Vector2);
MAKE_PTRARG(Rect2);
MAKE_PTRARG(Vector3);
MAKE_PTRARG(Matrix32);
MAKE_PTRARG(Plane);
MAKE_PTRARG(Quat);
MAKE_PTRARG(AABB);
MAKE_PTRARG(Matrix3);
MAKE_PTRARG(Transform);
MAKE_PTRARG(Color);
MAKE_PTRARG(Image);
MAKE_PTRARG(NodePath);
MAKE_PTRARG(RID);
MAKE_PTRARG(InputEvent);
MAKE_PTRARG(Dictionary);
MAKE_PTRARG(Array);
MAKE_PTRARG(ByteArray);
MAKE_PTRARG(IntArray);
MAKE_PTRARG(RealArray);
MAKE_PTRARG(StringArray);
MAKE_PTRARG(Vector2Array);
MAKE_PTRARG(Vector3Array);
MAKE_PTRARG(ColorArray);
MAKE_PTRARG(Variant);

//this is for Object

template <class T>
struct PtrToArg<T *> {

	_FORCE_INLINE_ static T *convert(const void *p_ptr) {

		return const_cast<T *>(reinterpret_cast<const T *>(p_ptr));
	}

	_FORCE_INLINE_ static void encode(T *p_var, void *p_ptr) {

		*((T **)p_ptr) = p_var;
	}
};

template <class T>
struct PtrToArg<const T *> {

	_FORCE_INLINE_ static const T *convert(const void *p_ptr) {

		return reinterpret_cast<const T *>(p_ptr);
	}

	_FORCE_INLINE_ static void encode(T *p_var, void *p_ptr) {

		*((T **)p_ptr) = p_var;
	}
};

//this is for the special cases used by Variant

#define MAKE_VECARG(m_type)                                                                \
	template <>                                                                            \
	struct PtrToArg<Vector<m_type> > {                                                     \
		_FORCE_INLINE_ static Vector<m_type> convert(const void *p_ptr) {                  \
			const DVector<m_type> *dvs = reinterpret_cast<const DVector<m_type> *>(p_ptr); \
			Vector<m_type> ret;                                                            \
			int len = dvs->size();                                                         \
			ret.resize(len);                                                               \
			{                                                                              \
				DVector<m_type>::Read r = dvs->read();                                     \
				for (int i = 0; i < len; i++) {                                            \
					ret[i] = r[i];                                                         \
				}                                                                          \
			}                                                                              \
			return ret;                                                                    \
		}                                                                                  \
		_FORCE_INLINE_ static void encode(Vector<m_type> p_vec, void *p_ptr) {             \
			DVector<m_type> *dv = reinterpret_cast<DVector<m_type> *>(p_ptr);              \
			int len = p_vec.size();                                                        \
			dv->resize(len);                                                               \
			{                                                                              \
				DVector<m_type>::Write w = dv->write();                                    \
				for (int i = 0; i < len; i++) {                                            \
					w[i] = p_vec[i];                                                       \
				}                                                                          \
			}                                                                              \
		}                                                                                  \
	};                                                                                     \
	template <>                                                                            \
	struct PtrToArg<const Vector<m_type> &> {                                              \
		_FORCE_INLINE_ static Vector<m_type> convert(const void *p_ptr) {                  \
			const DVector<m_type> *dvs = reinterpret_cast<const DVector<m_type> *>(p_ptr); \
			Vector<m_type> ret;                                                            \
			int len = dvs->size();                                                         \
			ret.resize(len);                                                               \
			{                                                                              \
				DVector<m_type>::Read r = dvs->read();                                     \
				for (int i = 0; i < len; i++) {                                            \
					ret[i] = r[i];                                                         \
				}                                                                          \
			}                                                                              \
			return ret;                                                                    \
		}                                                                                  \
	}

MAKE_VECARG(String);
MAKE_VECARG(uint8_t);
MAKE_VECARG(int);
MAKE_VECARG(float);
MAKE_VECARG(Vector2);
MAKE_VECARG(Vector3);
MAKE_VECARG(Color);

//for stuff that gets converted to Array vectors
#define MAKE_VECARR(m_type)                                                    \
	template <>                                                                \
	struct PtrToArg<Vector<m_type> > {                                         \
		_FORCE_INLINE_ static Vector<m_type> convert(const void *p_ptr) {      \
			const Array *arr = reinterpret_cast<const Array *>(p_ptr);         \
			Vector<m_type> ret;                                                \
			int len = arr->size();                                             \
			ret.resize(len);                                                   \
			for (int i = 0; i < len; i++) {                                    \
				ret[i] = (*arr)[i];                                            \
			}                                                                  \
			return ret;                                                        \
		}                                                                      \
		_FORCE_INLINE_ static void encode(Vector<m_type> p_vec, void *p_ptr) { \
			Array *arr = reinterpret_cast<Array *>(p_ptr);                     \
			int len = p_vec.size();                                            \
			arr->resize(len);                                                  \
			for (int i = 0; i < len; i++) {                                    \
				(*arr)[i] = p_vec[i];                                          \
			}                                                                  \
		}                                                                      \
	};                                                                         \
	template <>                                                                \
	struct PtrToArg<const Vector<m_type> &> {                                  \
		_FORCE_INLINE_ static Vector<m_type> convert(const void *p_ptr) {      \
			const Array *arr = reinterpret_cast<const Array *>(p_ptr);         \
			Vector<m_type> ret;                                                \
			int len = arr->size();                                             \
			ret.resize(len);                                                   \
			for (int i = 0; i < len; i++) {                                    \
				ret[i] = (*arr)[i];                                            \
			}                                                                  \
			return ret;                                                        \
		}                                                                      \
	}

MAKE_VECARR(Variant);
MAKE_VECARR(RID);
MAKE_VECARR(Plane);

#define MAKE_DVECARR(m_type)                                                    \
	template <>                                                                 \
	struct PtrToArg<DVector<m_type> > {                                         \
		_FORCE_INLINE_ static DVector<m_type> convert(const void *p_ptr) {      \
			const Array *arr = reinterpret_cast<const Array *>(p_ptr);          \
			DVector<m_type> ret;                                                \
			int len = arr->size();                                              \
			ret.resize(len);                                                    \
			{                                                                   \
				DVector<m_type>::Write w = ret.write();                         \
				for (int i = 0; i < len; i++) {                                 \
					w[i] = (*arr)[i];                                           \
				}                                                               \
			}                                                                   \
			return ret;                                                         \
		}                                                                       \
		_FORCE_INLINE_ static void encode(DVector<m_type> p_vec, void *p_ptr) { \
			Array *arr = reinterpret_cast<Array *>(p_ptr);                      \
			int len = p_vec.size();                                             \
			arr->resize(len);                                                   \
			{                                                                   \
				DVector<m_type>::Read r = p_vec.read();                         \
				for (int i = 0; i < len; i++) {                                 \
					(*arr)[i] = r[i];                                           \
				}                                                               \
			}                                                                   \
		}                                                                       \
	};                                                                          \
	template <>                                                                 \
	struct PtrToArg<const DVector<m_type> &> {                                  \
		_FORCE_INLINE_ static DVector<m_type> convert(const void *p_ptr) {      \
			const Array *arr = reinterpret_cast<const Array *>(p_ptr);          \
			DVector<m_type> ret;                                                \
			int len = arr->size();                                              \
			ret.resize(len);                                                    \
			{                                                                   \
				DVector<m_type>::Write w = ret.write();                         \
				for (int i = 0; i < len; i++) {                                 \
					w[i] = (*arr)[i];                                           \
				}                                                               \
			}                                                                   \
			return ret;                                                         \
		}                                                                       \
	}

MAKE_DVECARR(Plane);
//for special case StringName

#define MAKE_STRINGCONV(m_type)                                        \
	template <>                                                        \
	struct PtrToArg<m_type> {                                          \
		_FORCE_INLINE_ static m_type convert(const void *p_ptr) {      \
			m_type s = *reinterpret_cast<const String *>(p_ptr);       \
			return s;                                                  \
		}                                                              \
		_FORCE_INLINE_ static void encode(m_type p_vec, void *p_ptr) { \
			String *arr = reinterpret_cast<String *>(p_ptr);           \
			*arr = p_vec;                                              \
		}                                                              \
	};                                                                 \
                                                                       \
	template <>                                                        \
	struct PtrToArg<const m_type &> {                                  \
		_FORCE_INLINE_ static m_type convert(const void *p_ptr) {      \
			m_type s = *reinterpret_cast<const String *>(p_ptr);       \
			return s;                                                  \
		}                                                              \
	}

MAKE_STRINGCONV(StringName);
MAKE_STRINGCONV(IP_Address);

template <>
struct PtrToArg<DVector<Face3> > {
	_FORCE_INLINE_ static DVector<Face3> convert(const void *p_ptr) {
		const DVector<Vector3> *dvs = reinterpret_cast<const DVector<Vector3> *>(p_ptr);
		DVector<Face3> ret;
		int len = dvs->size() / 3;
		ret.resize(len);
		{
			DVector<Vector3>::Read r = dvs->read();
			DVector<Face3>::Write w = ret.write();
			for (int i = 0; i < len; i++) {
				w[i].vertex[0] = r[i * 3 + 0];
				w[i].vertex[1] = r[i * 3 + 1];
				w[i].vertex[2] = r[i * 3 + 2];
			}
		}
		return ret;
	}
	_FORCE_INLINE_ static void encode(DVector<Face3> p_vec, void *p_ptr) {
		DVector<Vector3> *arr = reinterpret_cast<DVector<Vector3> *>(p_ptr);
		int len = p_vec.size();
		arr->resize(len * 3);
		{
			DVector<Face3>::Read r = p_vec.read();
			DVector<Vector3>::Write w = arr->write();
			for (int i = 0; i < len; i++) {
				w[i * 3 + 0] = r[i].vertex[0];
				w[i * 3 + 1] = r[i].vertex[1];
				w[i * 3 + 2] = r[i].vertex[2];
			}
		}
	}
};
template <>
struct PtrToArg<const DVector<Face3> &> {
	_FORCE_INLINE_ static DVector<Face3> convert(const void *p_ptr) {
		const DVector<Vector3> *dvs = reinterpret_cast<const DVector<Vector3> *>(p_ptr);
		DVector<Face3> ret;
		int len = dvs->size() / 3;
		ret.resize(len);
		{
			DVector<Vector3>::Read r = dvs->read();
			DVector<Face3>::Write w = ret.write();
			for (int i = 0; i < len; i++) {
				w[i].vertex[0] = r[i * 3 + 0];
				w[i].vertex[1] = r[i * 3 + 1];
				w[i].vertex[2] = r[i * 3 + 2];
			}
		}
		return ret;
	}
};

#endif // METHOD_PTRCALL_H
#endif
