#include <fmt/core.h>

#include "format.hpp"
#include "stream.hpp"

namespace cppsl {

// Tracking system
void tracking_system::put(stream *S)
{
	active.insert(S);
}

void tracking_system::pop(stream *S)
{
	active.erase(S);
}

void tracking_system::inject(const stream &P, const stream *const original)
{
	for (stream *S : active) {
		if (S != original && S != &P)
			S->insert(S->end(), P.begin(), P.end());
	}
}

// Concatenation
stream concat(const stream &A, const stream &B)
{
	stream C;
	C.insert(C.begin(), A.begin(), A.end());
	C.insert(C.end(), B.begin(), B.end());
	return C;
}

stream concat(const stream &A, const instruction &I)
{
	stream C;
	C.insert(C.begin(), A.begin(), A.end());
	C.push_back(I);
	return C;
}

stream concat(const instruction &I, const stream &A)
{
	stream C;
	C.push_back(I);
	C.insert(C.end(), A.begin(), A.end());
	return C;
}

}