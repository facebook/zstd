# Cmake contributions

Contributions to the cmake build configurations are welcome. Please
use case sensitivity that matches modern (ie. cmake version 2.6 and above)
conventions of using lower-case for commands, and upper-case for
variables.

# CMake Style Recommendations

## Indent all code correctly, i.e. the body of

 * if/else/endif
 * foreach/endforeach
 * while/endwhile
 * macro/endmacro
 * function/endfunction

Use spaces for indenting, 2, 3 or 4 spaces preferably. Use the same amount of
spaces for indenting as is used in the rest of the file. Do not use tabs.

## Upper/lower casing

Most important: use consistent upper- or lowercasing within one file !

In general, the all-lowercase style is preferred.

So, this is recommended:

```
add_executable(foo foo.c)
```

These forms are discouraged

```
ADD_EXECUTABLE(bar bar.c)
Add_Executable(hello hello.c)
aDd_ExEcUtAbLe(blub blub.c)
```

## End commands
To make the code easier to read, use empty commands for endforeach(), endif(),
endfunction(), endmacro() and endwhile(). Also, use empty else() commands.

For example, do this:

```
if(FOOVAR)
   some_command(...)
else()
   another_command(...)
endif()
```

and not this:

```
if(BARVAR)
   some_other_command(...)
endif(BARVAR)
```

## Other resources for best practices

`https://cmake.org/cmake/help/latest/manual/cmake-developer.7.html#modules`
