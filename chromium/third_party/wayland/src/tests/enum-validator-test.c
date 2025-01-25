#include <assert.h>
#include "data/small-server-core.h"

int
main(int argc, char *argv[]) {
	assert(intf_A_foo_is_valid(INTF_A_FOO_FIRST, 1));
	assert(intf_A_foo_is_valid(INTF_A_FOO_FIRST, 2));

	assert(!intf_A_foo_is_valid(INTF_A_FOO_THIRD, 1));
	assert(intf_A_foo_is_valid(INTF_A_FOO_THIRD, 2));

	assert(intf_A_foo_is_valid(INTF_A_FOO_NEGATIVE, 2));
}
