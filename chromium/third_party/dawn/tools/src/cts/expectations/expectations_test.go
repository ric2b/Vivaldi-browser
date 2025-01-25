// Copyright 2024 The Dawn & Tint Authors
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice, this
//    list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
//
// 3. Neither the name of the copyright holder nor the names of its
//    contributors may be used to endorse or promote products derived from
//    this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

package expectations

import (
	"testing"

	"github.com/google/go-cmp/cmp"
)

// Tests behavior of Content.Format()
func TestContentFormat(t *testing.T) {
	// Intentionally includes extra whitespace since that should be removed.
	content := `# OS
# tags: [ linux mac win ]
# GPU
# tags: [ amd intel nvidia ]

################################################################################
# Bug order
################################################################################
crbug.com/3 [ linux ] a1 [ Failure ]
crbug.com/2 [ mac ] b1 [ Failure ]
crbug.com/1 [ win ] c1 [ Failure ]





################################################################################
# Query order
################################################################################
crbug.com/4 [ linux ] c2 [ Failure ]
crbug.com/4 [ linux ] b2 [ Failure ]
crbug.com/4 [ linux ] a2 [ Failure ]

################################################################################
# Tag order
################################################################################
crbug.com/5 [ intel ] a3 [ Failure ]
crbug.com/5 [ amd ] a3 [ Failure ]
crbug.com/5 [ nvidia ] a3 [ Failure ]

################################################################################
# Order priority
# Bug > Tags > Query
################################################################################
crbug.com/1 [ win ] z [ Failure ]
crbug.com/3 [ intel ] c [ Failure ]
crbug.com/2 [ intel ] a [ Failure ]
crbug.com/3 [ intel ] d [ Failure ]
crbug.com/2 [ amd ] b [ Failure ]
`

	expected_content := `# OS
# tags: [ linux mac win ]
# GPU
# tags: [ amd intel nvidia ]

################################################################################
# Bug order
################################################################################
crbug.com/1 [ win ] c1 [ Failure ]
crbug.com/2 [ mac ] b1 [ Failure ]
crbug.com/3 [ linux ] a1 [ Failure ]

################################################################################
# Query order
################################################################################
crbug.com/4 [ linux ] a2 [ Failure ]
crbug.com/4 [ linux ] b2 [ Failure ]
crbug.com/4 [ linux ] c2 [ Failure ]

################################################################################
# Tag order
################################################################################
crbug.com/5 [ amd ] a3 [ Failure ]
crbug.com/5 [ intel ] a3 [ Failure ]
crbug.com/5 [ nvidia ] a3 [ Failure ]

################################################################################
# Order priority
# Bug > Tags > Query
################################################################################
crbug.com/1 [ win ] z [ Failure ]
crbug.com/2 [ amd ] b [ Failure ]
crbug.com/2 [ intel ] a [ Failure ]
crbug.com/3 [ intel ] c [ Failure ]
crbug.com/3 [ intel ] d [ Failure ]
`
	expectations, err := Parse("", content)
	if err != nil {
		t.Errorf("Parsing content failed: %s", err.Error())
	}
	expectations.Format()

	if diff := cmp.Diff(expectations.String(), expected_content); diff != "" {
		t.Errorf("Format produced unexpected output: %v", diff)
	}
}
