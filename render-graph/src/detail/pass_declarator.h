#pragma once

#include <cstdint>

#include <caldera/pass_declarator.h>
#include "detail/structs.h"

struct caldera::PassDeclarator::Impl
{
    GraphDeclarator& declarator;
    uint32_t passIdx;
};