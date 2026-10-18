# Runtime Firebloom ranges provenance

- Saar Amar, `iBoot_firebloom`, commit
  `89990765efa8a886bbec590dd5ec2eff516e013e`: public research describing the
  32-byte bounded-pointer tuple, low-three-bit type tags, element-size
  encoding, pointer-count encoding, and checked cast failures. The repository
  has no declared software license, so this implementation uses the published
  facts and contains no copied source code.
- Microsoft GSL, commit `a75212b9f3b14162edd62d540cbf9273d5a59d20`,
  MIT License: public reference for span range and subspan invariants. The C++
  templates do not fit the freestanding C ABI, so the four-register
  representation is implemented directly.
- Boost Graph, commit `cd0a3be51cbdb0f45b3dbd4f90c37d530abdce51`,
  Boost Software License 1.0: the existing relation module supplies the reused
  union-find and path-compression engine.

Sources:

- https://github.com/saaramar/iBoot_firebloom/tree/89990765efa8a886bbec590dd5ec2eff516e013e
- https://saaramar.github.io/iBoot_firebloom/
- https://saaramar.github.io/iBoot_firebloom_type_desc/
- https://github.com/microsoft/GSL/blob/a75212b9f3b14162edd62d540cbf9273d5a59d20/include/gsl/span
- https://github.com/microsoft/GSL/blob/a75212b9f3b14162edd62d540cbf9273d5a59d20/LICENSE
- https://github.com/boostorg/graph/blob/cd0a3be51cbdb0f45b3dbd4f90c37d530abdce51/include/boost/pending/disjoint_sets.hpp
- https://github.com/boostorg/graph/blob/cd0a3be51cbdb0f45b3dbd4f90c37d530abdce51/LICENSE
