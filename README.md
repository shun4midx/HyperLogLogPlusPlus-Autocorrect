# HyperLogLogPlusPlus-Autocorrect

## NOTE: Specific details on how to install and run the programs in Python and C++ are will be in the `hllpp_py` and `hllpp_cpp` folders separately, and more information about the actual algorithm will be in `algo_description/description.pdf`

An HyperLogLog++-based autocorrection library developed from my earlier work on FQ-HLL Autocorrection, which used conventional HyperLogLog sketches.

This implementation uses the higher-precision sparse representation introduced by HLL++ for small q-gram sets, then converts sketches to conventional dense HLL registers when the sparse representation exceeds its promotion threshold. The empirical bias-correction tables used by the complete general-purpose HLL++ estimator are not currently included. Typical autocorrection workloads use very small q-gram sets and therefore remain in sparse mode, where those dense-estimator corrections are generally not used.

## Results
[Will be written later]

## License
HLLPP is licensed under the Apache-2.0 License, reference `LICENSE` for more information.