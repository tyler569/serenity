/*
 * Copyright (c) 2020, Matthew Olsson <matthewcolsson@gmail.com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <LibGfx/Path.h>
#include <LibWeb/DOM/Node.h>
#include <LibWeb/SVG/SVGElement.h>
#include <LibWeb/SVG/TagNames.h>

namespace Web::SVG {

struct SVGPaintingContext {
    Gfx::Color fill_color;
    Gfx::Color stroke_color;
    float stroke_width;
};

static const SVGPaintingContext default_painting_context = {
    Gfx::Color::Black,
    Gfx::Color::Black,
    1.0f
};

class SVGGraphicsElement : public SVGElement {
public:
    SVGGraphicsElement(Document&, const FlyString& tag_name);

    virtual void parse_attribute(const FlyString& name, const String& value) override;

    virtual void paint(Gfx::Painter& painter, const SVGPaintingContext& context) = 0;

    SVGPaintingContext make_painting_context_from(const SVGPaintingContext& context);

protected:
    Optional<Gfx::Color> m_fill_color;
    Optional<Gfx::Color> m_stroke_color;
    Optional<float> m_stroke_width;
};

}

namespace Web {

template<>
inline bool is<SVG::SVGGraphicsElement>(const Node& node)
{
    if (!is<Element>(node))
        return false;

    auto tag_name = to<Element>(node).tag_name();

#define __ENUMERATE_SVG_TAG(name) \
    if (tag_name == #name)        \
        return true;
    ENUMERATE_SVG_TAGS
#undef ENUMERATE_SVG_TAG

    return false;
}

}