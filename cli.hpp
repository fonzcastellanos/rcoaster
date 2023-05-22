#ifndef CLI_HPP
#define CLI_HPP

#define CLI_OPT_NAME_BUFFER_SIZE 64

namespace cli {

typedef unsigned int uint;

enum OptArgType {
  kOptArgType_Int,
  kOptArgType_Uint,
  kOptArgType_Float,
  kOptArgType_String
};

struct Opt {
  char name[CLI_OPT_NAME_BUFFER_SIZE];
  OptArgType arg_type;
  void *arg;
};

enum Status {
  kStatus_Ok,
  kStatus_InvalidOptArgType,
  kStatus_MissingOptArg,
  kStatus_InvalidOptArg,
  kStatus_UnexpectedOptName,
};

const char *StatusMessage(Status s);

/*
Parses options from arguments passed to the program.

Each option provided at the command-line has its name prefixed with "--" and has
an associated argument following its name. `--kmeans-cluster-count 10` is an
example.

Options must be before positional parameters. Otherwise, the intended
options would be considered positional parameters. For instance, in `encode
--kmeans-cluster-count 10 foreman.mp4`, the option `--kmeans-cluster-count 10`
is before the positional parameter `foreman.mp4`.

Input Parameters:
- argc:  number of arguments passed to the program
- argv: all arguments passed to the program
- opts_size: number of options

Output Parameters:
- opts: options
- argi: argument index that is one past that of the last option successfully
parsed. cannot be null
*/
Status ParseOpts(uint argc, char *argv[], Opt opts[], uint opts_size,
                 uint *argi);
}  // namespace cli

#endif  // CLI_HPP