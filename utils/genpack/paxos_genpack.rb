require_relative "lib/genpack"

# Generates paxos_type.h, paxos_type_pack.h, and paxos_type_pack.c

schema = GenPack::Schema.define "paxos_types" do
  typedef(:paxos_value) {
    string :paxos_value
  }
  message(:paxos_prepare) { 
    uint :iid
    uint :ballot
  }
  message(:paxos_promise) {
    uint :aid
    uint :iid
    uint :ballot
    uint :value_ballot
    paxos_value :value
  }
  message(:paxos_accept) {
    uint :iid
    uint :ballot
    paxos_value :value
  }
  message(:paxos_accepted) {
    uint :aid
    uint :iid
    uint :ballot
    uint :value_ballot
    paxos_value :value
  }
  message(:paxos_preempted) {
    uint :aid
    uint :iid
    uint :ballot
  }
  message(:paxos_repeat) {
    uint :from
    uint :to
  }
  message(:paxos_trim) {
    uint :iid
  }
  message(:paxos_acceptor_state) {
    uint :aid
    uint :trim_iid
  }
  message(:paxos_client_value) {
    paxos_value :value
  }
  union(:paxos_message) {
    paxos_prepare :prepare
    paxos_promise :promise
    paxos_accept :accept
    paxos_accepted :accepted
    paxos_preempted :preempted
    paxos_repeat :repeat
    paxos_trim :trim
    paxos_acceptor_state :state
    paxos_client_value :client_value
  }
end

LICENSE = <<-eos
/*
 * Copyright (c) 2013-2015, University of Lugano
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of the copyright holders nor the names of it
 *       contributors may be used to endorse or promote products derived from
 *       this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


eos

GenPack::CodeGenerator.generate(schema, LICENSE)
GenPack::HeaderGenerator.generate(schema, LICENSE)
GenPack::StructGenerator.generate(schema, LICENSE)
