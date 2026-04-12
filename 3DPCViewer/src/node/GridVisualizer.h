#pragma once

#include <osg/Group>
#include <osg/Geode>
#include <osg/Geometry>
#include <osg/StateSet>
#include <osg/Program>
#include <osg/Uniform>
#include <osg/BlendFunc>
#include <osg/Depth>

class Grid {
public:
    Grid(float size = 1000.0f) {
        _gridGeode = new osg::Geode();

        osg::ref_ptr<osg::Geometry> quad = new osg::Geometry();
        osg::ref_ptr<osg::Vec3Array> vertices = new osg::Vec3Array();

        float s = size;
        vertices->push_back(osg::Vec3(-s, -s, 0.0f));
        vertices->push_back(osg::Vec3(s, -s, 0.0f));
        vertices->push_back(osg::Vec3(s, s, 0.0f));
        vertices->push_back(osg::Vec3(-s, s, 0.0f));

        quad->setVertexArray(vertices);
        quad->addPrimitiveSet(new osg::DrawArrays(osg::PrimitiveSet::QUADS, 0, 4));

        osg::ref_ptr<osg::Program> program = new osg::Program;
        program->addShader(new osg::Shader(osg::Shader::VERTEX, vertSource));
        program->addShader(new osg::Shader(osg::Shader::FRAGMENT, fragSource));

        osg::StateSet* ss = quad->getOrCreateStateSet();
        ss->setAttributeAndModes(program, osg::StateAttribute::ON);

        ss->setAttributeAndModes(new osg::BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA));
        ss->setMode(GL_LIGHTING, osg::StateAttribute::OFF);

        ss->setRenderBinDetails(-1, "RenderBin");

        ss->addUniform(new osg::Uniform("gridSpacing", 1.0f));
        ss->addUniform(new osg::Uniform("lineWidth", 1.0f));
        ss->addUniform(new osg::Uniform("majorGridStep", 10.0f));
        ss->addUniform(new osg::Uniform("gridColor", osg::Vec4(0.5f, 0.5f, 0.5f, 0.6f)));
        ss->addUniform(new osg::Uniform("axisXColor", osg::Vec4(0.8f, 0.1f, 0.1f, 1.0f)));
        ss->addUniform(new osg::Uniform("axisYColor", osg::Vec4(0.1f, 0.8f, 0.1f, 1.0f)));
        ss->addUniform(new osg::Uniform("fadeRadius", size * 0.8f));

        _gridGeode->addDrawable(quad);
    }

    osg::ref_ptr<osg::Geode> getGridNode() const { return _gridGeode; }

private:
    osg::ref_ptr<osg::Geode> _gridGeode;

    const char* vertSource = R"(
        #version 330 core
        in vec4 osg_Vertex;
        uniform mat4 osg_ModelViewProjectionMatrix;
        out vec3 vWorldPos;
        void main() {
            vWorldPos = osg_Vertex.xyz;
            gl_Position = osg_ModelViewProjectionMatrix * osg_Vertex;
        }
    )";

    const char* fragSource = R"(
        #version 330 core
        in vec3 vWorldPos;
        uniform float gridSpacing;
        uniform float lineWidth;
        uniform float majorGridStep;
        uniform vec4 gridColor;
        uniform vec4 axisXColor;
        uniform vec4 axisYColor;
        uniform float fadeRadius;

        out vec4 fragColor;

        float getGrid(float pos, float spacing) {
            float coord = pos / spacing;
            float grid = abs(fract(coord - 0.5) - 0.5) / fwidth(coord);
            return 1.0 - smoothstep(0.0, lineWidth, grid);
        }

        void main() {
            float x = vWorldPos.x;
            float y = vWorldPos.y;

            float lineX = getGrid(x, gridSpacing);
            float lineY = getGrid(y, gridSpacing);
            float majorX = getGrid(x, gridSpacing * majorGridStep);
            float majorY = getGrid(y, gridSpacing * majorGridStep);

            float gridAlpha = max(lineX, lineY);
            float majorAlpha = max(majorX, majorY);

            vec4 finalColor = gridColor;
            if (majorAlpha > 0.1) {
                finalColor.rgb *= 1.5;
                gridAlpha = max(gridAlpha, majorAlpha);
            }

            float axisX = 1.0 - smoothstep(0.0, 0.05, abs(y));
            float axisY = 1.0 - smoothstep(0.0, 0.05, abs(x));

            if (axisX > 0.1) finalColor = mix(finalColor, axisXColor, axisX);
            if (axisY > 0.1) finalColor = mix(finalColor, axisYColor, axisY);

            float dist = length(vWorldPos.xy);
            float fade = 1.0 - smoothstep(fadeRadius * 0.5, fadeRadius, dist);

            float alpha = gridAlpha * finalColor.a * fade;
            if (alpha < 0.01) discard;

            fragColor = vec4(finalColor.rgb, alpha);
        }
    )";
};
