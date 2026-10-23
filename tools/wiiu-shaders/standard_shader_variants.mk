STANDARD_SHADER_VARIANTS := ForwardStandard ForwardStandardShadowed ShadowDepth

# The cooked inventory owns native embedding. A UI-only package has no Standard group.
STANDARD_SHADER_GROUP_SOURCES := $(foreach variant,$(STANDARD_SHADER_VARIANTS),$(WIIU_STANDARD_SHADER_SOURCES)/$(variant).vs $(WIIU_STANDARD_SHADER_SOURCES)/$(variant).ps)
STANDARD_SHADER_PRESENT_SOURCES := $(wildcard $(STANDARD_SHADER_GROUP_SOURCES))
STANDARD_SHADER_MATERIAL_MARKERS := $(wildcard $(CONTENT)/cooked/engine/materials/standard.hasset $(CONTENT)/wiiu_standard_material.hasset)
ifeq ($(strip $(STANDARD_SHADER_PRESENT_SOURCES)),)
ifneq ($(strip $(STANDARD_SHADER_MATERIAL_MARKERS)),)
$(error Wii U incomplete Standard shader group: packaged Standard material has no cooked shader sources)
endif
HELENGINE_WIIU_HAS_STANDARD_SHADERS := 0
STANDARD_SHADER_VARIANTS :=
else
ifneq ($(words $(STANDARD_SHADER_PRESENT_SOURCES)),$(words $(STANDARD_SHADER_GROUP_SOURCES)))
$(error Wii U incomplete Standard shader group: missing $(filter-out $(STANDARD_SHADER_PRESENT_SOURCES),$(STANDARD_SHADER_GROUP_SOURCES)))
endif
HELENGINE_WIIU_HAS_STANDARD_SHADERS := 1
endif
