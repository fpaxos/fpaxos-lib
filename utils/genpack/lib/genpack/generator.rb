#	Copyright (c) 2013, University of Lugano
#	All rights reserved.
#
#	Redistribution and use in source and binary forms, with or without
#	modification, are permitted provided that the following conditions are met:
#		* Redistributions of source code must retain the above copyright
#		  notice, this list of conditions and the following disclaimer.
#		* Redistributions in binary form must reproduce the above copyright
#		  notice, this list of conditions and the following disclaimer in the
#		  documentation and/or other materials provided with the distribution.
#		* Neither the name of the copyright holders nor the
#		  names of its contributors may be used to endorse or promote products
#		  derived from this software without specific prior written permission.
#
#	THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
#	AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE 
#	IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE 
#	ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY
#	DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
#	(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
#	LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
#	ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
#	(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF 
#	THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.


module GenPack
  module Utils
    def indent(line, level=1)
      ("\t" * level) << line << "\n"
    end
  end
  
  class Generator
    attr_reader :schema, :license
    include Utils
    def initialize(schema, license="")
      @schema = schema
      @license = license
    end
  end
  
  class CodeGenerator < Generator
    def self.generate(schema, license)
      gen = CodeGenerator.new(schema, license)
      File.open("#{schema.name}_pack.c", "w") do |f|
        gen.generate(f)
      end
    end
    def generate(f)
      f.write license
      f.write "#include \"#{schema.name}_pack.h\"\n\n"
      f.write Helpers::HELPERS
      schema.typedefs.each do |type|
        pack_typedef(f, type)
        unpack_typedef(f, type)
      end
      schema.messages.each do |msg|
        pack_message(f, msg)
        unpack_message(f, msg)
      end
      schema.unions.each do |union|
        pack_union(f, union)
        unpack_union(f, union)
      end
    end
    def pack_typedef(f, msg)
      wrap_function(f, msg.pack_signature) do
        msg.fields.each {|field| f.write indent(field.pack)}
      end
    end
    def unpack_typedef(f, msg)
      wrap_function(f, msg.unpack_signature) do
        msg.fields.each {|field| f.write indent(field.unpack("&v->"))}
      end
    end
    def pack_message(f, msg)
      wrap_function(f, msg.pack_signature) do
        f.write indent("msgpack_pack_array(p, #{msg.total_fields+1});")
        f.write indent("msgpack_pack_int32(p, #{msg.name.to_s.upcase});")
        msg.fields.each {|field| f.write indent(field.pack)}
      end
    end
    def unpack_message(f, msg)
      wrap_function(f, msg.unpack_signature) do
        f.write indent("int i = 1;")
        msg.fields.each {|field| f.write indent(field.unpack("&v->"))}
      end
    end
    def pack_union(f, union)
      wrap_function(f, union.pack_signature) do
        f.write indent("switch (v->type) {")
        union.fields.each do |field|
          f.write indent("case " << field.var_type.ctype.to_s.upcase << ":", 1)
          f.write indent(field.pack("&v->u."), 2)
          f.write indent("break;", 2)
         end
         f.write indent("}", 1)
     end
    end
    def unpack_union(f, union)
      wrap_function(f, union.unpack_signature) do
        f.write indent("v->type = MSGPACK_OBJECT_AT(o,0).u64;")
        f.write indent("switch (v->type) {")
        union.fields.each do |field|
          f.write indent("case " << field.var_type.ctype.to_s.upcase << ":", 1)
          f.write indent(field.unpack("&v->u."), 2)
          f.write indent("break;", 2)
         end
         f.write indent("}", 1)
     end
    end
    def wrap_function(f, signature)
      f.write "#{signature}\n{\n"
      yield
      f.write "}\n\n"
    end
  end

  class HeaderGenerator < Generator
    def self.generate(schema, license)
      gen = HeaderGenerator.new(schema, license)
      File.open("#{schema.name}_pack.h", "w") do |f|
        f.write license
        gen.wrap_header(f) do
          gen.declarations(f)
        end
      end
    end
    def wrap_header(f)
      f.puts "#ifndef _#{schema.name.upcase}_PACK_H_"
      f.puts "#define _#{schema.name.upcase}_PACK_H_\n\n"
      f.puts "#include \"#{schema.name}.h\""
      f.puts "#include <msgpack.h>\n\n"
      yield
      f.puts "\n#endif"
    end
    def declarations(f)
      (schema.messages + schema.unions).each do |type|
        f.puts "#{type.pack_signature};"
        f.puts "#{type.unpack_signature};"
      end
    end
  end
  
  class StructGenerator < Generator
    def self.generate(schema, license)
      gen = StructGenerator.new(schema, license)
      File.open("#{schema.name}.h", "w") do |f|
        f.write license
        gen.wrap_header(f) do
          gen.declare_types(f)
        end
      end
    end
    def wrap_header(f)
      f.write "#ifndef _#{schema.name.upcase}_H_\n"
      f.write "#define _#{schema.name.upcase}_H_\n\n"
      f.write "#include <stdint.h>\n\n"
      yield
      f.puts "#endif\n"
    end
    def declare_types(f)      
      (schema.typedefs + schema.messages).each do |type|
        declare_struct(f, type)
      end
      schema.unions.each do |union|
        declare_enum(f, union)
        declare_union(f, union)
      end
    end
    def declare_struct(f, type)
      wrap_struct(f, type) do
        type.fields.each do |field| 
          f.write indent("#{field.var_type.declare(field.name)}")
        end
      end
    end
    def declare_enum(f,type)
      f.puts "enum #{type.name}_type\n{\n"
      all_fields = type.fields.collect do |field|
        "\t#{field.var_type.ctype.to_s.upcase}"
      end
      f.puts all_fields.join(",\n")
      f.puts "};"
      f.puts "typedef enum #{type.name}_type #{type.name}_type;\n\n"
    end
    def declare_union(f, type)
      wrap_struct(f, type) do
        f.write indent("#{type.name}_type type;")
        f.write indent("union\n\t{")
        type.fields.each {|field| f.puts "\t\t#{field.var_type.declare(field.name)}\n"}
        f.write indent("} u;")
      end
    end
    def wrap_struct(f, type)
      f.puts "struct #{type.name}\n{\n"
      yield
      f.puts "};\n"
      f.puts "typedef struct #{type.name} #{type.name};\n\n"
    end
  end
  
  module Helpers
    HELPERS = <<-eos
#define MSGPACK_OBJECT_AT(obj, i) (obj->via.array.ptr[i].via)


static void msgpack_pack_string(msgpack_packer* p, char* buffer, int len)
{
\t#if MSGPACK_VERSION_MAJOR > 0
\tmsgpack_pack_bin(p, len);
\tmsgpack_pack_bin_body(p, buffer, len);
\t#else
\tmsgpack_pack_raw(p, len);
\tmsgpack_pack_raw_body(p, buffer, len);
\t#endif
}

static void msgpack_unpack_int32_at(msgpack_object* o, int32_t* v, int* i)
{
\t*v = (int32_t)MSGPACK_OBJECT_AT(o,*i).u64;
\t(*i)++;
}

static void msgpack_unpack_uint32_at(msgpack_object* o, uint32_t* v, int* i)
{
\t*v = (uint32_t)MSGPACK_OBJECT_AT(o,*i).u64;
\t(*i)++;
}

static void msgpack_unpack_string_at(msgpack_object* o, char** buffer, int* len, int* i)
{
\t*buffer = NULL;
\t#if MSGPACK_VERSION_MAJOR > 0
\t*len = MSGPACK_OBJECT_AT(o,*i).bin.size;
\tconst char* obj = MSGPACK_OBJECT_AT(o,*i).bin.ptr;
\t#else
\t*len = MSGPACK_OBJECT_AT(o,*i).raw.size;
\tconst char* obj = MSGPACK_OBJECT_AT(o,*i).raw.ptr;
\t#endif
\tif (*len > 0) {
\t\t*buffer = malloc(*len);
\t\tmemcpy(*buffer, obj, *len);
\t}
\t(*i)++;
}

eos
  end
end
