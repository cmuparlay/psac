
# PSAC++
A library for parallel self-adjusting computation in C++. This library is part of the following research project. If you build on it for scientific purposes, please cite it as follows.

> **Efficient Parallel Self-Adjusting Computation**<br />
> Daniel Anderson, Guy E. Blelloch, Anubhav Baweja, Umut A. Acar<br />
> The 33rd ACM Symposium on Parallelism in Algorithms and Architectures (SPAA 21), 2021

## Building the examples

To build the example programs, you will [CMake](https://cmake.org/) and a recent C++ compiler. [LLVM/Clang](https://clang.llvm.org/) is recommended. With the repository cloned, create a build directory in the repository root (call it anything you want, but `build` is the standard). From this build directory, run `cmake ..` and then run `make`. You will find the example programs in the `examples` directory.

The source code for the example runners can be seen in the `examples` directory in the project root, although this code mostly just sets up the input and then calls the implemented example applications, which can be found in the `include/psac/examples` directory. Looking through these will provide a broad range of examples to get you familiar with the structure of self-adjusting programs.

## Writing a basic program

To write programs using PSAC++, you just need to learn a few simple primitives. Your program should contain the include declaration `#include <psac/psac.hpp>`.

If built outside the CMake environment, you will need to specify the required compiler options:
1. enable C++17 with `-std=c++17`
2. link against system threads, e.g. `-pthreads`
3. enable 16-byte CAS if available with `-mcx16`
4. ensure that the PSAC headers can be located by the compiler, either by putting them in your include path, directing the compiler to them with an include flag `-I/path/to/psac/headers/`, or copying the headers directly into your project. 

To simply things, it is recommended to stay within the provided CMake environment.

### Modifiables
Input variables to a self-adjusting program should be contained in *modifiable* references (modifiables for short). Modifiables are denoted by wrapping their enclosing type with the `psac::Mod<>` template. The value of a modifiable can be written to with the psac_write primitive.

```c++
psac::Mod<int> x;
psac_write(&x, 5);
```

Outside of a self-adjusting computation, there are no restrictions, but within self-adjusting code, modifiables must be written only once, and they must be written to before they are read. Modifiables that are written to before executing a self-adjusting computation may be read but should not be written to within the self-adjusting computation.

### Writing self-adjusting computations

Self-adjusting computations are denoted by functions declared using the `psac_function` macro. The `psac_function` macro takes as arguments, the name of the function, followed by the argument list. To read a modifiable within a self-adjusting computation, use the `psac_read` function. `psac_read` takes as arguments:
1.  a bracket enclosed list of variable declarations (to be treated as function arguments) which will be initialized with the values of the given modifiables at the time of reading, 
2. a bracket-enclosed list of pointers to modifiables that will be read,
3. a function body which is to be executed.

Hopefully an example will make this clearer.

```c++

psac::Mod<int> input;
psac::Mod<int> output;

// Declare a self-adjusting function called add_one, which reads
// the modifiable called "input", and writes its value plus one
// to the modifiable called "output"
psac_function(add_one) {
  psac_read((int x), (&input), {
    psac_write(&output, x + 1);
  });
}
```

To invoke a self-adjusting computation, we call the `psac_run` function, like so. `psac_run` takes as arguments, the values of the parameters to be passed to the self-adjusting computation (if any). It returns a handle to the computation which can be used to later propagate updates the computation.

```c++
int main() {
  psac_write(&input, 5);
  auto computation = psac_run(add_one);
  assert(output.value == 6);
}
```

Outside of a self-adjusting computation, the value of a modifiable can be inspected by looking at the `value` field. `psac_read` should only be used inside of a self-adjusting computation.

To propagate an update, write to an input modifiable, and then call `psac_propagate`.

```c++
int main() {
  psac_write(&input, 5);
  auto computation = psac_run(add_one);
  assert(output.value == 6);
  
  psac_write(&input, 10);
  psac_propagate(computation);
  assert(output.value == 11);  
}
```

### Returning values via destination passing

Self-adjusting computations can not explicitly return values (they are internally implemented as void functions), so if a return value is desired, an empty modifiable should be passed as an argument, and used to store the return value. Modifiables that are passed to self-adjusting functions should be passed via pointers, not by value or by reference.

```c++
psac_function(add_one, psac::Mod<int>* input, psac::Mod<int>* output) {
  psac_read((int x), (input), {
    psac_write(output, x+1);
  });
}
```

This function can then be used like so.

```c++
int main() {
  psac::Mod<int> input, output;
  psac_write(&input, 5);
  auto computation = psac_run(add_one, &input, &output);
  assert(output.value == 6);
  
  psac_write(&input, 10);
  psac_propagate(computation);
  assert(result.value == 11);  
}
```

Note that `input` is used as an input modifiable, i.e. its value was already written before executing the self-adjusting computation, and is subsequently read by the computation, while `output` is used as an output parameter, i.e. it is empty before executing the computation, and is filled by the computation.

### Calling subroutines

Self-adjusting functions can call other self-adjusting functions, and even themselves to perform recursion. To do so, use the `psac_call` function, which functions pretty much the same as `psac_run`, except that it can only be called when already inside a self-adjusting computation.

```c++
psac_function(add_x, int x, psac::Mod<int>* input, psac::Mod<int>* output) {
  psac_read((int y), (input), {
    psac_write(output, y + x);
  });
}

// Implements add_one by calling add_x with 1 as the argument
psac_function(add_one, psac::Mod<int>* input, psac::Mod<int>* output) {
  psac_call(add_x, 1, input, output);
}
```

### Dynamically allocating modifiables

Sometimes, it might be desirable for a computation to allocate modifiables dynamically, rather than to pre-allocate them before execution. This can be achieved using the `psac_alloc` function, which takes as an argument, the type to be stored in the modifiable. Modifiables allocated in this way can never be visible to the outside world (outside of the self-adjusting computation), so they are only used to store intermediate values.

```c++
psac_function(times_two_add_one, psac::Mod<int>* input, psac::Mod<int>* output) {
  // Temp will store the result of input + 1
  psac::Mod<int>* temp = psac_alloc(int);
  psac_call(add_one, input, temp);
  
  psac_read((int x), (temp), {
    psac_write(output, 2 * x);
  });
}
```

### Parallelism

Of course, for parallel self-adjusting computation, we need some parallelism. There are two ways to perform parallel computation: A parallel fork operation that runs two blocks of code in parallel, and a parallel for loop, which runs a block of code over a range of values in parallel.

Here is an example of a divide-and-conquer sum algorithm that uses a parallel fork.

```c++
template<typename It>
psac_function(sum, It lo, It hi, psac::Mod<int>* result) {
  if (lo == hi - 1) {
    psac_read((auto x), (lo), {
      psac_write(result, x); });
  }
  else {
    auto mid = lo + (hi - lo) / 2;
    auto left_res = psac_alloc(int);
    auto right_res = psac_alloc(int);
    psac_par(
      { psac_call(sum, lo, mid, left_res); },
      { psac_call(sum, mid, hi, right_res); }
    );
    psac_read((auto x, auto y), (left_res, right_res), {
      psac_write(result, x + y); });
  }
}
```

A parallel for loop takes as arguments, a variable declaration for the loop counter, a lower bound and an upper bound, the loop granularity, and a function block. Here is an algorithm that takes a pair of ranges of modifiables, and writes, for each element in the first range, its value plus one into the corresponding position in the second range.

```c++
template<typename It>
psac_function(map_add_one, It in_begin, It in_end, It out_begin, It out_end) {
  auto size = in_end - in_begin;
  psac_parallel_for(int i, 0, size, 512, {
    psac_read((int x, int y), (in_begin + i, out_begin + i), {
      psac_write(y, x + 1);
    });
  });
}
```

The loop granularity is size of the chunks of the loop that should be ran sequentially.