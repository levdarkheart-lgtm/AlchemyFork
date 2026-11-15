/**
 * @file pipeline_group_restore_tuple_native.inl
 * @brief Owner-free static inputs used only after a child/proof has retired.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 * Included inside ALSceneGroupPublicationNative.
 */
// This is not a render packet. It has no native/viewer owner, camera matrix,
// special-pass state or inactive slot capacity. Expanding it is dirty-only;
// warm cohort views keep consuming their existing sealed source certificate.
struct StaticRestoreTuple
{
    const ALMetalBuffer* vertices=nullptr;
    const ALMetalBuffer* indices=nullptr;
    std::size_t position_offset=0,normal_offset=0,texcoord_offset=0,texcoord1_offset=0,texcoord2_offset=0;
    std::size_t color_offset=0,emissive_offset=0,tangent_offset=0,index_offset=0;
    U32 vertex_count=0,index_count=0,material_alpha_mode=0;
    ALMetalIndexType index_type=ALMetalIndexType::uint16;
    std::vector<const ALMetalTexture*> diffuse;
    std::vector<ALMetalWorldMaterialSlot> material;
    std::vector<ALMetalWorldPBRSlot> pbr;
    std::array<F32,16> texture_matrix{};
    F32 alpha_cutoff=0.f;
    bool alpha_mask=false,double_sided=false,use_vertex_color=true,alpha_cutoff_bias=true;

    explicit StaticRestoreTuple(const ALMetalWorldSimpleBatch& source)
        :vertices(source.vertices),indices(source.indices),position_offset(source.position_offset),
         normal_offset(source.normal_offset),texcoord_offset(source.texcoord_offset),
         texcoord1_offset(source.texcoord1_offset),texcoord2_offset(source.texcoord2_offset),
         color_offset(source.color_offset),emissive_offset(source.emissive_offset),tangent_offset(source.tangent_offset),
         index_offset(source.index_offset),vertex_count(source.vertex_count),index_count(source.index_count),
         material_alpha_mode(source.material_alpha_mode),index_type(source.index_type),alpha_cutoff(source.alpha_cutoff),
         alpha_mask(source.alpha_mask),double_sided(source.double_sided),use_vertex_color(source.use_vertex_color),
         alpha_cutoff_bias(source.alpha_cutoff_bias)
    {
        diffuse.assign(source.diffuse_textures.begin(),source.diffuse_textures.begin()+source.texture_count);
        if(source.material_slot_count)
            material.assign(source.materialSlots().begin(),source.materialSlots().begin()+source.material_slot_count);
        if(source.pbr_slot_count)
            pbr.assign(source.pbrSlots().begin(),source.pbrSlots().begin()+source.pbr_slot_count);
        std::copy_n(source.texture_matrix,texture_matrix.size(),texture_matrix.begin());
    }
    U64 ownedBytes() const noexcept
    {
        return sizeof(*this)+U64(diffuse.capacity())*sizeof(diffuse[0])+
            U64(material.capacity())*sizeof(material[0])+U64(pbr.capacity())*sizeof(pbr[0]);
    }
    U64 valueBytes() const noexcept
    {
        return sizeof(*this)+U64(diffuse.size())*sizeof(diffuse[0])+
            U64(material.size())*sizeof(material[0])+U64(pbr.size())*sizeof(pbr[0]);
    }
    void expand(ALMetalWorldSimpleBatch& destination) const
    {
        destination=ALMetalWorldSimpleBatch{};
        destination.vertices=vertices;destination.indices=indices;
        destination.position_offset=position_offset;destination.normal_offset=normal_offset;
        destination.texcoord_offset=texcoord_offset;destination.texcoord1_offset=texcoord1_offset;destination.texcoord2_offset=texcoord2_offset;
        destination.color_offset=color_offset;destination.emissive_offset=emissive_offset;destination.tangent_offset=tangent_offset;
        destination.index_offset=index_offset;destination.vertex_count=vertex_count;destination.index_count=index_count;destination.index_type=index_type;
        destination.material_alpha_mode=material_alpha_mode;destination.alpha_cutoff=alpha_cutoff;
        destination.alpha_mask=alpha_mask;destination.double_sided=double_sided;
        destination.use_vertex_color=use_vertex_color;destination.alpha_cutoff_bias=alpha_cutoff_bias;
        destination.texture_count=static_cast<U32>(diffuse.size());
        std::copy(diffuse.begin(),diffuse.end(),destination.diffuse_textures.begin());
        destination.material_slot_count=static_cast<U32>(material.size());destination.pbr_slot_count=static_cast<U32>(pbr.size());
        if(!material.empty())std::copy(material.begin(),material.end(),destination.materialSlots().begin());
        if(!pbr.empty())std::copy(pbr.begin(),pbr.end(),destination.pbrSlots().begin());
        std::copy(texture_matrix.begin(),texture_matrix.end(),destination.texture_matrix);
    }
};
// Owner-only failure seam; no setting, worker callback or GPU owner uses it.
inline void (*staticRestorationAllocationForTesting)()=nullptr;
