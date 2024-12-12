#include <Util.h>

#include <fstream>

std::string slurp(std::string_view path)
{
    constexpr auto read_size = std::size_t(4096);
    auto stream = std::ifstream(path.data());
    stream.exceptions(std::ios_base::badbit);

    if (!stream) {
        throw std::ios_base::failure("File does not exist");
    }

    auto out = std::string();
    auto buf = std::string(read_size, '\0');
    while (stream.read(& buf[0], read_size)) {
        out.append(buf, 0, stream.gcount());
    }

    out.append(buf, 0, stream.gcount());
    return out;
}
