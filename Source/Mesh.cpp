#include "Mesh.h"

#include <iostream>

#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>

bool Mesh::loadFromObj(const char* filename)
{
	tinyobj::attrib_t attrib;
	std::vector<tinyobj::shape_t> shapes;
	std::vector<tinyobj::material_t> materials;

	std::string warn;
    std::string err;

    tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, filename, "../Assets/");

    if(!warn.empty())
    {
        std::cout << "WARN: " << warn << std::endl;
    }

    if(!err.empty())
    {
        std::cerr << err << std::endl;
        return false;
    }

    for(size_t s = 0; s < shapes.size(); s++)
    {
        size_t index_offset = 0;
        for(size_t f = 0; f < shapes[s].mesh.num_face_vertices.size(); f++)
        {
            int fv = 3;
            for(size_t v = 0; v < fv; v++)
            {
                tinyobj::index_t idx = shapes[s].mesh.indices[index_offset + v];

                tinyobj::real_t vx = attrib.vertices[3 * idx.vertex_index + 0];
                tinyobj::real_t vy = attrib.vertices[3 * idx.vertex_index + 1];
                tinyobj::real_t vz = attrib.vertices[3 * idx.vertex_index + 2];

                
                tinyobj::real_t nx = attrib.normals[3 * idx.normal_index + 0];
                tinyobj::real_t ny = attrib.normals[3 * idx.normal_index + 1];
                tinyobj::real_t nz = attrib.normals[3 * idx.normal_index + 2];

                tinyobj::real_t ux = attrib.texcoords[2 * idx.texcoord_index + 0];
                tinyobj::real_t uy = attrib.texcoords[2 * idx.texcoord_index + 1];
                
                Vertex new_vert =
                {
                    {vx, vy, vz},
                    {nx, ny, nz},
                     {nx, ny, nz},
                    {ux, 1-uy}
                };

                _vertices.push_back(new_vert);
            }
            index_offset += fv;
        }
    }

	return true;
}