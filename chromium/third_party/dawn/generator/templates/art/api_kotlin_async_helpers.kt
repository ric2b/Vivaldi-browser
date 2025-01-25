//* Copyright 2024 The Dawn & Tint Authors
//*
//* Redistribution and use in source and binary forms, with or without
//* modification, are permitted provided that the following conditions are met:
//*
//* 1. Redistributions of source code must retain the above copyright notice, this
//*    list of conditions and the following disclaimer.
//*
//* 2. Redistributions in binary form must reproduce the above copyright notice,
//*    this list of conditions and the following disclaimer in the documentation
//*    and/or other materials provided with the distribution.
//*
//* 3. Neither the name of the copyright holder nor the names of its
//*    contributors may be used to endorse or promote products derived from
//*    this software without specific prior written permission.
//*
//* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
//* AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
//* IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
//* DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
//* FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
//* DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
//* SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
//* CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
//* OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
//* OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
package {{ kotlin_package }}

import kotlin.coroutines.resume
import kotlin.coroutines.suspendCoroutine
{% from 'art/api_kotlin_types.kt' import kotlin_declaration, kotlin_definition with context %}

//* We make a return class for every function pointer so that usage of callback-using methods can be
// replaced with suspend (async) function that returns the same data.
{% for function_pointer
        in by_category['function pointer'] if len(function_pointer.name.chunks) > 1 %}
    //* Function pointers generally end in Callback which we replace with Return.
    {% set return_name = function_pointer.name.chunks[:-1] | map('title') | join + 'Return' %}
    data class {{ return_name }}(
        {% for arg in kotlin_record_members(function_pointer.arguments) %}
            val {{ as_varName(arg.name) }}: {{ kotlin_declaration(arg) }},
        {% endfor %})
{% endfor %}

//* Every method that is identified as using callbacks is given a helper method that wraps the
//* call with a suspend function.
{% for obj in by_category['object'] %}
    {% for method in obj.methods if is_async_method(method) %}
        {% set function_pointer = method.arguments[-2].type %}
        {% set return_name = function_pointer.name.chunks[:-1] | map('title') | join + 'Return' %}
        suspend fun {{ obj.name.CamelCase() }}.{{ method.name.camelCase() }}(
            {%- for arg in method.arguments[:-2] %}
                {{- as_varName(arg.name) }}: {{ kotlin_definition(arg) }},
            {%- endfor %}) = suspendCoroutine {
                {{ method.name.camelCase() }}(
                    {%- for arg in method.arguments[:-2] %}
                        {{- as_varName(arg.name) }},
                    {% endfor %}) {
                    {%- for arg in kotlin_record_members(function_pointer.arguments) %}
                        {{- as_varName(arg.name) }},
                    {%- endfor %} -> it.resume({{ return_name }}(
                        {%- for arg in kotlin_record_members(function_pointer.arguments) %}
                            {{- as_varName(arg.name) }},
                        {%- endfor %})
                    )
                }
            }
    {% endfor %}
{% endfor %}
