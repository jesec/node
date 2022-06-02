# Maintaining Single Executable Applications support

Support for [single executable applications](https://github.com/nodejs/node/blob/master/doc/contributing/technical-priorities.md#single-executable-applications)
is one of the key technical priorities identified for the success of Node.js.

## High level strategy

From the [next-10 discussions](https://github.com/nodejs/next-10/blob/main/meetings/summit-nov-2021.md#single-executable-applications)
there are 2 approaches the project believes are important to support:

* Compile with Node.js into executable (`boxnode` approach).
* Bundle into existing Node.js executable (`pkg` approach).

### Compile with node into executable

No additional code within the Node.js project is needed to support the
option of compiling a bundled application along with Node.js into a single
executable application.

### Bundle into existing Node.js executable

The project does not plan to provide the complete solution but instead the key
elements which are required in the Node.js executable in order to enable
bundling with the pre-built Node.js binaries. This includes:

* Looking for a segment within the executable that holds bundled code.
* Running the bundled code when such a segment is found.

It is left up to external tools/solutions to:

* Bundle the code into a single script or implement a prelude/loader
  script that loads the entrypoint.
* Add a segment to an existing Node.js executable which contains
  the appropriate header, Node options and embedded script.
* Re-generate or removing signatures on the resulting executable
* Provide a virtual file system, and hooking it in if needed to
  support native modules or reading file contents.

## Maintaining

### Compile with node into executable

The approach of compiling with node into an executable requires that we
maintain a stable [em-bedder API](https://nodejs.org/dist/latest/docs/api/embedding.html).

### Bundle into existing Node.js executable

The following header must be appended to the executable in order to have it
run as a single executable application:

JSCODEVVVVVVVVFFFFFFFFF

where:

* `VVVVVVVV` represents the version to be used to interpret the section,
  for example `00000001`.
* `FFFFFFFF` represents the flags to be used in the process of starting
  the bundled application. Currently this must be `00000000` to indicate that
  no flags are set.

The characters in both `VVVVVVVV` and `FFFFFFFF` are restricted to being
hexadecimal characters (`0` through `9` and `A` through `F`) that form
a 32-bit, big endian integer.

The string following the header is treated as a set of command line options
for Node.js. After bundling, the embedded script would exclusively process
the command line options, and it would not be possible to pass options to
Node.js. For example, to bake in `--no-warnings`:

```text
JSCODE0000000100000000--no-warnings
```

The following section is embedded script, separated from the header with
`\NUL`:

\NUL SCRIPT_SIZE \NUL SCRIPT

where:

* `SCRIPT_SIZE` is positive integer string, representing the size of the
  script, for example `123`.
* `SCRIPT` is the embedded script.

For example, a embedded hello world script with no baked-in node options:

```text
JSCODE0000000100000000\NUL40\NULconsole.log("Hello from single binary");
```

Support for bundling into existing Node.js binaries is maintained
in `src/node_single_executable.*`.

Currently only POSIX-compliant platforms are supported. The goal
is to expand this to include Windows and macOS as well.

If a breaking change to the content after the header is required, the version
`VVVVVVVV` should be incremented. Support for a new format
may be introduced as a semver-minor provided that older versions
are still supported. Removing support for a version is semver-major.

The `FFFFFFFF` is a set of flags that is used to control the
process of starting the application. For example they might indicate
that some set of arguments should be suppressed on the command line.
Currently no flags are in use.
