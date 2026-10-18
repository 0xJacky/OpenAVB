# Runtime Firebloom relations provenance

The implementation uses union-by-rank and full path compression as described
by Boost Graph's disjoint-set implementation at commit
`cd0a3be51cbdb0f45b3dbd4f90c37d530abdce51` (BSL-1.0):

- https://github.com/boostorg/graph/blob/cd0a3be51cbdb0f45b3dbd4f90c37d530abdce51/include/boost/pending/disjoint_sets.hpp
- https://github.com/boostorg/graph/blob/cd0a3be51cbdb0f45b3dbd4f90c37d530abdce51/LICENSE
- https://github.com/boostorg/graph/commit/cd0a3be51cbdb0f45b3dbd4f90c37d530abdce51

OpenAVB implements the AVP interface's low-three-bit type-kind tag, relation
result states, directed fallback, backlink lifecycle, dynamic relation set,
open addressing cache, metadata fields, and 64-bit pair hash. No Boost source
text is copied into this implementation.
