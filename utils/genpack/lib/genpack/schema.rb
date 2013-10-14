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
  class Schema
    attr_accessor :name, :types
    def initialize(name)
      @name = name
      @types = []
      @license = ""
    end
    def self.define(name, &block)
      schema = Schema.new(name)
      schema.instance_eval(&block)
      schema
    end
    def typedef(name, &block)
      typedef = TypeDef.new(self, name)
      typedef.instance_eval(&block)
      types << typedef
    end
    def message(name, &block)
      message = Message.new(self, name)
      message.instance_eval(&block)
      types << message
    end
    def union(name, &block)
      union = Union.new(self, name)
      union.instance_eval(&block)
      types << union
    end
    def typedefs
      types.select {|t| t.is_a?(TypeDef)}
    end
    def messages
      types.select {|t| t.is_a?(Message)}
    end
    def unions
      types.select {|t| t.is_a?(Union)}
    end
  end
  
  class Compound
    attr_accessor :schema, :name, :fields
    def initialize(schema, name)
      @schema = schema
      @name = name
      @fields = []
    end
    def int(name)
      fields << Field.new(Types::Int.new, name)
    end
    def uint(name)
      fields << Field.new(Types::UInt.new, name)
    end
    def string(name)
      fields << Field.new(Types::String.new, name)
    end
    def method_missing(method, *arguments, &block)
      t = schema.types.detect {|t| t.name == method}
      if t.nil?
        super
      else
        if t.is_a?(TypeDef)
          fields << Field.new(Types::CustomTypeDef.new(t.name), arguments[0])
        else
          fields << Field.new(Types::CustomType.new(t.name), arguments[0])
        end
      end
    end
    def pack_signature
      "void msgpack_pack_#{name}(msgpack_packer* p, #{name}* v)"
    end
    def unpack_signature
      "void msgpack_unpack_#{name}(msgpack_object* o, #{name}* v)"
    end
    def total_fields
      count = 0
      fields.each do |f| 
        t = schema.types.detect {|t| t.name == f.name}
        if t.nil?
          count += 1
        else
          count += t.fields.size
        end
      end
      count
    end
  end
  
  class Message < Compound; end
  class Union < Compound; end
  class TypeDef < Compound;
    def pack_signature
      "static void msgpack_pack_#{name}(msgpack_packer* p, #{name}* v)"
    end
    def unpack_signature
      "static void msgpack_unpack_#{name}_at(msgpack_object* o, #{name}* v, int* i)"
    end
  end

  class Field
    attr_accessor :var_type, :name
    def initialize(var_type, name)
      @var_type = var_type
      @name = name
    end
    def pack(access="v->")
      if var_type.is_a?(Types::CustomTypeDef)
        access = "&" + access
      end
      var_type.pack(name, access)
    end
    def unpack(access="v->")
      var_type.unpack(name, access)
    end
  end
  
  module Types
    class Type
      def pack(name, access)
        "msgpack_pack_#{ctype}(p, #{access}#{name});"
      end
      def unpack(name, access)
        "msgpack_unpack_#{ctype}_at(o, #{access}#{name}, &i);"
      end
      def declare(name)
        "#{ctype} #{name};"
      end
    end
    class Int < Type
      def ctype 
        "int32"
      end
      def declare(name)
        "#{ctype}_t #{name};"
      end
    end
    class UInt < Int
      def ctype; "uint32"; end
    end
    class String < Type
      def pack(name, access)
        "msgpack_pack_string(p, #{access}#{name}_val, #{access}#{name}_len);"
      end
      def unpack(name, access)
        "msgpack_unpack_string_at(o, #{access}#{name}_val, #{access}#{name}_len, i);"
      end
      def declare(name)
        "int #{name.to_s}_len;\n\tchar *#{name.to_s}_val;"
      end
    end
    class CustomType < Type
      attr_accessor :ctype
      def initialize(ctype)
        @ctype = ctype
      end
      def pack(name, access)
        "msgpack_pack_#{ctype}(p, #{access}#{name});"
      end
      def unpack(name, access)
        "msgpack_unpack_#{ctype}(o, #{access}#{name});"
      end
    end
    class CustomTypeDef < Type
      attr_accessor :ctype
      def initialize(ctype)
        @ctype = ctype
      end
    end
  end
end
