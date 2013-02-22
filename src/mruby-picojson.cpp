#include "../picojson.h"
#include <mruby.h>
#include <mruby/array.h>
#include <mruby/data.h>
#include <mruby/hash.h>
#include <mruby/string.h>

namespace {

template<class T>
void deleter(mrb_state* mrb, void* ptr) {
  reinterpret_cast<T*>(ptr)->~T();
  mrb_free(mrb, ptr);
}

template<class T>
char const* get_class_name();

template<>
char const* get_class_name<std::string>() { return "cxx_string"; }
template<>
char const* get_class_name<picojson::value>() { return "picojson_value"; }

template<class T>
mrb_data_type& data_type() {
  static mrb_data_type ret = { get_class_name<T>(), &deleter<T> };
  return ret;
}

template<class T>
T& create(mrb_state* mrb) {
  void* const ptr = mrb_malloc(mrb, sizeof(T));
  mrb_data_object_alloc(mrb, mrb->object_class, ptr, &data_type<T>());
  return *(new(ptr) T());
}

// forward decl
mrb_value to_mruby(mrb_state* mrb, picojson::value const& json);

mrb_value to_mruby(mrb_state* mrb, picojson::array const& ary) {
  mrb_value ret = mrb_ary_new_capa(mrb, ary.size());
  for(size_t i = 0; i < ary.size(); ++i) {
    mrb_ary_set(mrb, ret, i, to_mruby(mrb, ary[i]));
  }
  return ret;
}
mrb_value to_mruby(mrb_state* mrb, picojson::object const& obj) {
  mrb_value ret = mrb_hash_new_capa(mrb, obj.size());
  for(picojson::object::const_iterator i = obj.begin(); i != obj.end(); ++i) {
    mrb_hash_set(mrb, ret, mrb_symbol_value(mrb_intern(mrb, i->first.c_str())),
                 to_mruby(mrb, i->second));
  }
  return ret;
}
mrb_value to_mruby(mrb_state* mrb, std::string const& str) {
  return mrb_str_new(mrb, str.c_str(), str.size());
}
mrb_value to_mruby(double const& d) {
  return (d == std::floor(d))
      ? mrb_fixnum_value(d) : mrb_float_value(d);
}

mrb_value to_mruby(mrb_state* mrb, picojson::value const& json) {
  return
      json.is<picojson::null>()? mrb_nil_value():
      json.is<double>()? to_mruby(json.get<double>()):
      json.is<bool>()? (json.get<bool>()? mrb_true_value() : mrb_false_value()):
      json.is<std::string>()? to_mruby(mrb, json.get<std::string>()):
      json.is<picojson::array>()? to_mruby(mrb, json.get<picojson::array>()):
      json.is<picojson::object>()? to_mruby(mrb, json.get<picojson::object>()):
      (mrb_raise(mrb, E_RUNTIME_ERROR, "unknown picojson type"), mrb_nil_value());
}

void to_cxx(mrb_state* mrb, mrb_value const& v, picojson::value& ret) {
  switch(mrb_type(v)) {
    case MRB_TT_FALSE: ret = picojson::value(false); break;
    case MRB_TT_TRUE: ret = picojson::value(true); break;
    case MRB_TT_FIXNUM: ret = picojson::value(double(mrb_fixnum(v))); break;
    case MRB_TT_SYMBOL: picojson::value(mrb_sym2name(mrb, mrb_symbol(v))).swap(ret); break;
    case MRB_TT_STRING: ret = picojson::value(std::string(RSTRING_PTR(v), RSTRING_LEN(v))); break;
    case MRB_TT_FLOAT: ret = picojson::value(double(mrb_float(v))); break;
    case MRB_TT_ARRAY: {
      picojson::value(picojson::array_type, bool()).swap(ret);
      picojson::array& ary = ret.get<picojson::array>();
      ary.resize(RARRAY_LEN(v));
      for(size_t i = 0; i < ary.size(); ++i) {
        to_cxx(mrb, RARRAY_PTR(v)[i], ary[i]);
      }
    } break;
    case MRB_TT_HASH: {
      picojson::value(picojson::object_type, bool()).swap(ret);
      picojson::object& obj = ret.get<picojson::object>();
      mrb_value keys = mrb_hash_keys(mrb, v);
      for(size_t i = 0; i < RARRAY_LEN(keys); ++i) {
        mrb_value const& k = RARRAY_PTR(keys)[i];
        mrb_value const str_k = mrb_funcall(mrb, k, "to_s", 0);
        if(mrb_string_p(str_k)) {
          std::string& str = create<std::string>(mrb);
          str.assign(RSTRING_PTR(str_k), RSTRING_LEN(str_k));
          to_cxx(mrb, mrb_hash_get(mrb, v, k), obj[str]);
        } else {
          mrb_raise(mrb, E_RUNTIME_ERROR, "invalid key");
          return;
        }
      }
    } break;
#ifdef ENABLE_STRUCT
    case MRB_TT_STRUCT:
      break;
#endif
    default:
      if(mrb_respond_to(mrb, v, mrb_intern(mrb, "to_picojson"))) {
        to_cxx(mrb, mrb_funcall(mrb, v, "to_picojson", 0), ret);
      } else {
        mrb_raisef(mrb, E_RUNTIME_ERROR, "unsupported data type: %s", mrb_obj_classname(mrb, v));
        ret = picojson::value();
      }
  }
}

mrb_value parse(mrb_state* mrb, mrb_value self) {
  char const* str = NULL; int str_len;
  mrb_get_args(mrb, "z", &str, &str_len);
  assert(str);

  picojson::value& v = create<picojson::value>(mrb);
  std::string const err = picojson::parse(v, str, str + str_len);
  return err.empty()? to_mruby(mrb, v) :
      (mrb_raise(mrb, E_RUNTIME_ERROR, err.c_str()), self);
}

mrb_value stringify(mrb_state* mrb, mrb_value /* self */) {
  mrb_value obj;
  mrb_get_args(mrb, "o", &obj);

  picojson::value& v = create<picojson::value>(mrb);
  to_cxx(mrb, obj, v);
  std::string& ret = create<std::string>(mrb);
  v.serialize().swap(ret);
  return mrb_str_new(mrb, ret.c_str(), ret.size());
}

}

extern "C" {
  void mrb_mruby_picojson_gem_init(mrb_state* mrb) {
    RClass* const pico = mrb_define_module(mrb, "Picojson");
    mrb_define_class_method(mrb, pico, "parse", &parse, ARGS_REQ(1));
    mrb_define_class_method(mrb, pico, "stringify", &stringify, ARGS_REQ(1));
  }

  void mrb_mruby_picojson_gem_final(mrb_state* mrb) {
  }
}
