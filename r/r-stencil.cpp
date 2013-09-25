#include <stencila/stencil.hpp>
using namespace Stencila;

#include "r-extension.hpp"
#include "r-workspace.hpp"

STENCILA_R_FUNC Stencil_new(void){
    STENCILA_R_BEGIN
        return STENCILA_R_TO(Stencil,new Stencil);
    STENCILA_R_END
}

STENCILA_R_FUNC Stencil_id(SEXP self){
    STENCILA_R_BEGIN
        return wrap(from<Stencil>(self).id());
    STENCILA_R_END
}

STENCILA_R_FUNC Stencil_load(SEXP self, SEXP content){
    STENCILA_R_BEGIN
        from<Stencil>(self).load(as<std::string>(content));
        return nil;
    STENCILA_R_END
}

STENCILA_R_FUNC Stencil_append_html(SEXP self, SEXP html){
    STENCILA_R_BEGIN
        from<Stencil>(self).append_html(as<std::string>(html));
        return nil;
    STENCILA_R_END
}

STENCILA_R_FUNC Stencil_dump(SEXP self){
    STENCILA_R_BEGIN
        return wrap(from<Stencil>(self).dump());
    STENCILA_R_END
}

STENCILA_R_FUNC Stencil_render(SEXP self,SEXP workspace){
    STENCILA_R_BEGIN
        RWorkspace rworkspace(workspace);
        from<Stencil>(self).render(rworkspace);
        return nil;
    STENCILA_R_END
}
