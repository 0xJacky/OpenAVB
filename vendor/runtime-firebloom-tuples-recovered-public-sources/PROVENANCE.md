# Runtime Firebloom tuple provenance

The bounded-range contracts follow Microsoft GSL `span` at commit
`a75212b9f3b14162edd62d540cbf9273d5a59d20` under the MIT License:

- https://github.com/microsoft/GSL/blob/a75212b9f3b14162edd62d540cbf9273d5a59d20/include/gsl/span
- https://github.com/microsoft/GSL/blob/a75212b9f3b14162edd62d540cbf9273d5a59d20/LICENSE

The canonical-type relationship engine reused by tuple validation follows
Boost Graph's disjoint-set implementation at commit
`cd0a3be51cbdb0f45b3dbd4f90c37d530abdce51` under BSL-1.0:

- https://github.com/boostorg/graph/blob/cd0a3be51cbdb0f45b3dbd4f90c37d530abdce51/include/boost/pending/disjoint_sets.hpp
- https://github.com/boostorg/graph/blob/cd0a3be51cbdb0f45b3dbd4f90c37d530abdce51/LICENSE

Saar Amar's public iBoot Firebloom research at commit
`89990765efa8a886bbec590dd5ec2eff516e013e` documents the 32-byte tuple,
tagged type identities, and fatal checked-pointer semantics:

- https://github.com/saaramar/iBoot_firebloom/tree/89990765efa8a886bbec590dd5ec2eff516e013e
- https://saaramar.github.io/iBoot_firebloom/
- https://saaramar.github.io/iBoot_firebloom_type_desc/

That repository has no declared software license. This implementation uses
the published facts and contains no copied source text. The tuple ABI,
canonical type identities, aggregate-result locations, register save and
restore meaning, comparison direction, and fixed byte counts follow the
expected ROM interface.
