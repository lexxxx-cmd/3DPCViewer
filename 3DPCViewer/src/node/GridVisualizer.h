#ifndef GRID_H
#define GRID_H

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

        // 1. 创建几何体（一个覆盖地面的大矩形）
        osg::ref_ptr<osg::Geometry> quad = new osg::Geometry();
        osg::ref_ptr<osg::Vec3Array> vertices = new osg::Vec3Array();

        float s = size;
        vertices->push_back(osg::Vec3(-s, -s, 0.0f));
        vertices->push_back(osg::Vec3(s, -s, 0.0f));
        vertices->push_back(osg::Vec3(s, s, 0.0f));
        vertices->push_back(osg::Vec3(-s, s, 0.0f));

        quad->setVertexArray(vertices);
        quad->addPrimitiveSet(new osg::DrawArrays(osg::PrimitiveSet::QUADS, 0, 4));

        // 2. 配置着色器程序 (兼容 Qt6 Core Profile)
        osg::ref_ptr<osg::Program> program = new osg::Program;
        program->addShader(new osg::Shader(osg::Shader::VERTEX, vertSource));
        program->addShader(new osg::Shader(osg::Shader::FRAGMENT, fragSource));

        osg::StateSet* ss = quad->getOrCreateStateSet();
        ss->setAttributeAndModes(program, osg::StateAttribute::ON);

        // 3. 配置渲染状态
        // 开启混合以支持透明淡出
        ss->setAttributeAndModes(new osg::BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA));
        ss->setMode(GL_LIGHTING, osg::StateAttribute::OFF);

        // 确保网格不会遮挡透明物体，通常放在较早的渲染阶段
        ss->setRenderBinDetails(-1, "RenderBin");

        // 4. 设置 Uniform 参数
        ss->addUniform(new osg::Uniform("gridSpacing", 1.0f));     // 基础格网间距
        ss->addUniform(new osg::Uniform("lineWidth", 1.0f));       // 线条平滑宽度
        ss->addUniform(new osg::Uniform("majorGridStep", 10.0f));  // 主网格步长
        ss->addUniform(new osg::Uniform("gridColor", osg::Vec4(0.5f, 0.5f, 0.5f, 0.6f)));
        ss->addUniform(new osg::Uniform("axisXColor", osg::Vec4(0.8f, 0.1f, 0.1f, 1.0f)));
        ss->addUniform(new osg::Uniform("axisYColor", osg::Vec4(0.1f, 0.8f, 0.1f, 1.0f)));
        ss->addUniform(new osg::Uniform("fadeRadius", size * 0.8f)); // 边缘淡出起始距离

        _gridGeode->addDrawable(quad);
    }

    osg::ref_ptr<osg::Geode> getGridNode() const { return _gridGeode; }

private:
    osg::ref_ptr<osg::Geode> _gridGeode;

    // --- Vertex Shader (GLSL 330) ---
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

    // --- Fragment Shader (GLSL 330) ---
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
            // 使用 fwidth 实现抗锯齿的网格线
            float coord = pos / spacing;
            float grid = abs(fract(coord - 0.5) - 0.5) / fwidth(coord);
            return 1.0 - smoothstep(0.0, lineWidth, grid);
        }

        void main() {
            float x = vWorldPos.x;
            float y = vWorldPos.y;

            // 1. 计算基础网格和主网格
            float lineX = getGrid(x, gridSpacing);
            float lineY = getGrid(y, gridSpacing);
            float majorX = getGrid(x, gridSpacing * majorGridStep);
            float majorY = getGrid(y, gridSpacing * majorGridStep);

            float gridAlpha = max(lineX, lineY);
            float majorAlpha = max(majorX, majorY);

            // 2. 颜色混合
            vec4 finalColor = gridColor;
            if (majorAlpha > 0.1) {
                finalColor.rgb *= 1.5; // 主网格亮度提升
                gridAlpha = max(gridAlpha, majorAlpha);
            }

            // 3. 坐标轴突出显示 (X轴红色, Y轴绿色)
            float axisX = 1.0 - smoothstep(0.0, 0.05, abs(y));
            float axisY = 1.0 - smoothstep(0.0, 0.05, abs(x));
            
            if (axisX > 0.1) finalColor = mix(finalColor, axisXColor, axisX);
            if (axisY > 0.1) finalColor = mix(finalColor, axisYColor, axisY);

            // 4. 边缘淡出效果
            float dist = length(vWorldPos.xy);
            float fade = 1.0 - smoothstep(fadeRadius * 0.5, fadeRadius, dist);

            float alpha = gridAlpha * finalColor.a * fade;
            if (alpha < 0.01) discard; // 性能优化：丢弃全透明片元

            fragColor = vec4(finalColor.rgb, alpha);
        }
    )";
};

#endif