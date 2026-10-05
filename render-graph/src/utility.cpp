#include "detail/utility.h"

#include <exception>
#include <iostream>
#include <ostream>

#include "caldera/structs.h"

namespace caldera::detail
{
    int family_to_index(FamilyType const family) noexcept
    {
        static_assert(FamilyType::graphics < FamilyType::transfer);
        static_assert(FamilyType::transfer < FamilyType::compute);

        assert(FamilyType::graphics <= family && family <= FamilyType::compute);
        return static_cast<int>(family);
    }

    void fatal(const char* msg)
    {
        std::cerr << msg << std::endl;
        std::terminate();
    }

    void fatal(std::string const& msg) {
        fatal(msg.c_str());
    }

    void assert_result(
        vk::Result const result,
        const char* const failedMsg)
    {
        if (result < vk::Result::eSuccess) {
            std::cerr << failedMsg << " (vk::Result: " << vk::to_string(result) << ")" << std::endl;
            std::terminate();
        }
    }

    void assert_result(vk::Result result, std::string const& failedMsg) {
        assert_result(result, failedMsg.c_str());
    }
}
