#include <cstdio>
#include <cstring>

#include "cli.hpp"

namespace cli {

const char* const kStatusMessages[] = {
    "success", "invalid option argument type", "missing option argument",
    "invalid option argument", "unexpected option name"};

const char* StatusMessage(Status s) { return kStatusMessages[s]; }

Status ParseOpts(uint argc, char* argv[], Opt opts[], uint opts_size,
                 uint* argi) {
  uint i = 1;

  while (i < argc && std::strncmp(argv[i], "--", 2) == 0) {
    if (std::strcmp(argv[i], "--") == 0) {
      ++i;
      break;
    }

    if (i + 1 >= argc) {
      *argi = i;
      return kStatus_MissingOptArg;
    }

    uint j = 0;

    while (j < opts_size && std::strcmp(&argv[i][2], opts[j].name) != 0) {
      ++j;
    }
    if (j == opts_size) {
      *argi = i;
      return kStatus_UnexpectedOptName;
    }

    const char* format;
    switch (opts[j].arg_type) {
      case kOptArgType_Int: {
        format = "%d";
        break;
      }
      case kOptArgType_Uint: {
        format = "%u";
        break;
      }
      case kOptArgType_Float: {
        format = "%f";
        break;
      }
      case kOptArgType_String: {
        format = "%s";
        break;
      }
      default: {
        *argi = i;
        return kStatus_InvalidOptArgType;
      }
    }

    int rc = std::sscanf(argv[i + 1], format, opts[j].arg);
    if (rc != 1) {
      *argi = i;
      return kStatus_InvalidOptArg;
    }

    i += 2;
  }

  *argi = i;

  return kStatus_Ok;
}
}  // namespace cli