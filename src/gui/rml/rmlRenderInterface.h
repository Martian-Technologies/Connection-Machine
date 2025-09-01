#ifndef rmlRenderInterface_h
#define rmlRenderInterface_h

#include <RmlUi/Core/RenderInterface.h>

#include "gpu/mainRendererDefs.h"

// -- Inherited from Rml::RenderInterface --
class RmlRenderInterface : public Rml::RenderInterface {
public:
	inline void setWindowToRenderOn(WindowId windowId) { currentWindowId = windowId; };

	Rml::CompiledGeometryHandle CompileGeometry(Rml::Span<const Rml::Vertex> vertices, Rml::Span<const int> indices) override;
	void ReleaseGeometry(Rml::CompiledGeometryHandle geometry) override;
	void RenderGeometry(Rml::CompiledGeometryHandle handle, Rml::Vector2f translation, Rml::TextureHandle texture) override;

	Rml::TextureHandle LoadTexture(Rml::Vector2i& texture_dimensions, const Rml::String& source) override;
	Rml::TextureHandle GenerateTexture(Rml::Span<const Rml::byte> source, Rml::Vector2i source_dimensions) override;
	void ReleaseTexture(Rml::TextureHandle texture_handle) override;

	void EnableScissorRegion(bool enable) override;
	void SetScissorRegion(Rml::Rectanglei region) override;

private:
	WindowId currentWindowId = 0;
};

#endif
