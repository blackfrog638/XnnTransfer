#include <sstream>
#include <string>
#include <vector>

#include <boost/archive/iterators/base64_from_binary.hpp>
#include <boost/archive/iterators/binary_from_base64.hpp>
#include <boost/archive/iterators/ostream_iterator.hpp>
#include <boost/archive/iterators/transform_width.hpp>

inline std::string base64_encode(const std::vector<char>& data) {
    using namespace boost::archive::iterators;
    std::stringstream ss;
    using Base64Iterator = base64_from_binary<transform_width<const char*, 6, 8>>;
    std::copy(Base64Iterator(data.data()), Base64Iterator(data.data() + data.size()),
              boost::archive::iterators::ostream_iterator<char>(ss));
    return ss.str();
}

inline std::vector<char> base64_decode(const std::string& encoded_string) {
    using namespace boost::archive::iterators;
    std::string clean_encoded = encoded_string;

    clean_encoded.erase(std::remove_if(clean_encoded.begin(), clean_encoded.end(), ::isspace), clean_encoded.end());

    using Base64DecoderIterator = transform_width<binary_from_base64<std::string::const_iterator>, 8, 6>;
    std::vector<char> decoded_data;
    try {
        std::copy(Base64DecoderIterator(clean_encoded.begin()), Base64DecoderIterator(clean_encoded.end()),
                  std::back_inserter(decoded_data));
    } catch (const boost::archive::iterators::dataflow_exception& e) {
        throw std::runtime_error(std::string("Base64 decoding dataflow error: ") + e.what() +
                                 ". Input was: " + encoded_string.substr(0, 50) + "...");
    } catch (const std::exception& e) {
        throw std::runtime_error(std::string("Base64 decoding error: ") + e.what() +
                                 ". Input was: " + encoded_string.substr(0, 50) + "...");
    }
    return decoded_data;
}