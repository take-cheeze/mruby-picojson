assert("true stringify") { Picojson.stringify(true) == "true" }
assert("false stringify") { Picojson.stringify(false) == "false" }
assert("integer stringify") { Picojson.stringify(42.0) == "42" }
assert("float stringify") { Picojson.stringify(41.5) == "41.5" }
assert("string stringify") { Picojson.stringify("hello") == '"hello"' }

assert("true parse") { Picojson.parse("true") == true }
assert("false parse") { Picojson.parse("false") == false }
assert("numeric parse") { Picojson.parse("41.5") == 41.5 }
assert("string parse") {
  Picojson.parse('"hello"') == 'hello' and
  Picojson.parse("\"\\\"\\\\\\/\\b\\f\\n\\r\\t\"") == "\"\\/\b\f\n\r\t" and
  Picojson.parse("\"\\u0061\\u30af\\u30ea\\u30b9\"") == "a\xe3\x82\xaf\xe3\x83\xaa\xe3\x82\xb9" and
  Picojson.parse("\"\\ud840\\udc0b\"") == "\xf0\xa0\x80\x8b"
}

# assert("number limit") {
#   ret = true
#   a = 1.0
#   (1...1024).each {|i|
#     b = Picojson.parse(Picojson.stringify(a))
#     if((i < 53 and a != b) or (a - b).abs / b > 1e-8)
#       ret = false
#     end
#     a *= 2
#   }
#   ret
# }

assert("empty array") {
  v = Picojson.parse("[]")
  v.instance_of? Array and v.empty?
}

assert("hash array") {
  v = Picojson.parse("{}")
  v.instance_of? Hash and v.empty?
}

assert("array") {
  v = Picojson.parse("[1,true,\"hello\"]")
  v == [1.0, true, "hello"] and v.length == 3
}

assert("hash") {
  v = Picojson.parse("{ \"a\": true }")
  v == { :a => true } and not v.key?(:z)
}

assert("syntax error") {
  ret = true
  begin
    Picojson.parse("falsoa")
  rescue => det
    ret = ret and det.to_s == "syntax error at line 1 near: oa"
  end

  begin
    Picojson.parse("{]");
  rescue => det
    ret = ret and det.to_s == "syntax error at line 1 near: ]"
  end

  begin
    Picojson.parse("\n\bbell")
  rescue => det
    ret = ret and det.to_s == "syntax error at line 2 near: bell"
  end

  begin
    Picojson.parse("\"abc\nd\"")
  rescue => det
    ret = ret and det.to_s == "syntax error at line 1 near: "
  end

  ret
}

assert("deep equal") {
  v1 = Picojson.parse("{ \"b\": true, \"a\": [1,2,\"three\"], \"d\": 2 }")
  v2 = Picojson.parse("{ \"d\": 2.0, \"b\": true, \"a\": [1,2,\"three\"] }")
  v1 == v2
}

assert("deep not equal") {
  v1 = Picojson.parse("{ \"b\": true, \"a\": [1,2,\"three\"], \"d\": 2 }")
  v2 = Picojson.parse("{ \"d\": 2.0, \"a\": [1,\"three\"], \"b\": true }")
  v1 != v2
}

assert("delete compare") {
  v = Picojson.parse("{ \"b\": true, \"a\": [1,2,\"three\"], \"d\": 2 }")
  v.delete :b
  v[:a].delete "three"
  v == Picojson.parse("{ \"a\": [1,2], \"d\": 2 }")
}
