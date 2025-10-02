#pragma once

#include "define.hpp"
#include "TextureManager.hpp"

// Return a representative texture layer for coloring particles of a given block
inline TextureType textureForBlockColor(BlockType b)
{
    switch (b)
    {
        case STONE:   return T_STONE;
        case COBBLE:  return T_COBBLE;
        case DIRT:    return T_DIRT;
        case GRASS:   return T_GRASS_TOP; // use top tint for grass
        case SAND:    return T_SAND;
        case SNOW:    return T_SNOW;
        case LOG:     return T_LOG_SIDE;
        case LEAF:    return T_LEAF;
        case BEDROCK: return T_BEDROCK;
        case WATER:   return T_WATER;
        default:      return T_DIRT;
    }
}

