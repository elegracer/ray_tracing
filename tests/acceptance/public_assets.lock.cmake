set(RT_PUBLIC_ACCEPTANCE_ASSET_SCHEMA "public_acceptance_assets_v1")
set(RT_PUBLIC_ACCEPTANCE_REQUIRED_GATES
    "usd_stage_import"
    "openpbr_material_compile"
    "realtime_render"
    "offline_render_artifacts"
    "multi_pose_coverage"
    "camera_model_coverage"
    "simultaneous_multiview"
    "deterministic_reference_image"
)
set(RT_PUBLIC_ACCEPTANCE_RENDER_OUTPUT_SCHEMA "public_acceptance_render_outputs_v1")
set(RT_PUBLIC_ACCEPTANCE_RENDER_WIDTH 640)
set(RT_PUBLIC_ACCEPTANCE_RENDER_HEIGHT 480)
set(RT_PUBLIC_ACCEPTANCE_RENDER_FORMATS
    "linear_exr"
    "display_png"
)
set(RT_PUBLIC_ACCEPTANCE_MIN_VIEW_OUTPUTS 10)
set(RT_PUBLIC_ACCEPTANCE_MIN_IMAGE_ARTIFACTS 20)
set(RT_PUBLIC_ACCEPTANCE_MANIFEST_FIELDS
    "source_revisions"
    "render_settings"
    "sample_seed"
    "cameras"
    "outputs"
    "simultaneous_submission_id"
    "reference_metrics"
)
set(RT_PUBLIC_ACCEPTANCE_CAMERA_MODELS
    "pinhole32"
    "equi62_lut1d"
)
set(RT_PUBLIC_ACCEPTANCE_POSES
    "front_three_quarter"
    "rear_three_quarter"
    "elevated_three_quarter"
)
set(RT_PUBLIC_ACCEPTANCE_SINGLE_VIEW_CASES
    "front_three_quarter:pinhole32"
    "front_three_quarter:equi62_lut1d"
    "rear_three_quarter:pinhole32"
    "rear_three_quarter:equi62_lut1d"
    "elevated_three_quarter:pinhole32"
    "elevated_three_quarter:equi62_lut1d"
)
set(RT_PUBLIC_ACCEPTANCE_MULTIVIEW_CASE_ID "orbit_4_mixed_models")
set(RT_PUBLIC_ACCEPTANCE_MULTIVIEW_CAMERA_COUNT 4)
set(RT_PUBLIC_ACCEPTANCE_MULTIVIEW_AZIMUTHS_DEG
    45
    135
    225
    315
)
set(RT_PUBLIC_ACCEPTANCE_MULTIVIEW_CAMERA_MODELS
    "pinhole32"
    "equi62_lut1d"
    "pinhole32"
    "equi62_lut1d"
)
set(RT_PUBLIC_ACCEPTANCE_ASSET_IDS
    "usd_wg_vehicles"
    "openpbr_examples"
)

set(RT_PUBLIC_ACCEPTANCE_ASSET_usd_wg_vehicles_REPOSITORY "usd-wg/assets")
set(RT_PUBLIC_ACCEPTANCE_ASSET_usd_wg_vehicles_REVISION
    "1b91f3c464891af259d51d9ee9ee9e6c357f7079")
set(RT_PUBLIC_ACCEPTANCE_ASSET_usd_wg_vehicles_SPARSE_PATH "full_assets/Vehicles")
set(RT_PUBLIC_ACCEPTANCE_ASSET_usd_wg_vehicles_TREE
    "c61f53549ae4eb9f3145ae8526005937ab61abcb")
set(RT_PUBLIC_ACCEPTANCE_ASSET_usd_wg_vehicles_TREE_LISTING_SHA256
    "3fc919dc29c20e2fd19dbe2c65cbf6cc7ae23f74373a46911e74ca5d1f43d4a3")
set(RT_PUBLIC_ACCEPTANCE_ASSET_usd_wg_vehicles_CACHE_DIR "usd-wg-assets-1b91f3c4")
set(RT_PUBLIC_ACCEPTANCE_ASSET_usd_wg_vehicles_ENTRYPOINT
    "full_assets/Vehicles/USD_Mini_Car_Kit/assets/vehicles/vehicleVariants.usda")
set(RT_PUBLIC_ACCEPTANCE_ASSET_usd_wg_vehicles_LICENSE "CC0-1.0")
set(RT_PUBLIC_ACCEPTANCE_ASSET_usd_wg_vehicles_FILE_COUNT 197)
set(RT_PUBLIC_ACCEPTANCE_ASSET_usd_wg_vehicles_BYTE_COUNT 5818479)
set(RT_PUBLIC_ACCEPTANCE_ASSET_usd_wg_vehicles_COVERAGE
    "composed_layers"
    "references"
    "variants"
    "mesh_geometry"
    "usd_preview_surface"
    "textures"
)
set(RT_PUBLIC_ACCEPTANCE_ASSET_usd_wg_vehicles_FILES
    "full_assets/Vehicles/readme.md"
    "8f9cb49f8e898e8a61bde597f2eea73a6f3b86db49f15d5c3cacb8a02b5a154a"
    "full_assets/Vehicles/USD_Mini_Car_Kit/assets/vehicles/vehicleVariants.usda"
    "a43447c1152127a45b079a791a966aaef75dadab0029170d7fb457f4dfd2fa1f"
    "full_assets/Vehicles/USD_Mini_Car_Kit/assets/vehicles/tractor/asset/tractorFullAsset.usda"
    "411842d2f757275b1b2210f1511dfae4357f3e3ff1d9f734e62aa36cbabd2445"
    "full_assets/Vehicles/USD_Mini_Car_Kit/materials/red.usda"
    "49dbe6bc8fbbf50f9ca4f813f4d9f8dff5fa0a07bfef11787f6bd9057ce7c49f"
)

set(RT_PUBLIC_ACCEPTANCE_ASSET_openpbr_examples_REPOSITORY
    "AcademySoftwareFoundation/OpenPBR")
set(RT_PUBLIC_ACCEPTANCE_ASSET_openpbr_examples_REVISION
    "f8d6d947dfae4c9b599965a86c22826ea7a8dbfb")
set(RT_PUBLIC_ACCEPTANCE_ASSET_openpbr_examples_SPARSE_PATH "examples")
set(RT_PUBLIC_ACCEPTANCE_ASSET_openpbr_examples_TREE
    "7ef59b7d640997019c9f6f2d23e8ff90d6e4ede5")
set(RT_PUBLIC_ACCEPTANCE_ASSET_openpbr_examples_TREE_LISTING_SHA256
    "ba9288e9b1d8d90e8951b7862652c57b40b248e8356ab99c4cf826939cd28ea7")
set(RT_PUBLIC_ACCEPTANCE_ASSET_openpbr_examples_CACHE_DIR "openpbr-f8d6d947")
set(RT_PUBLIC_ACCEPTANCE_ASSET_openpbr_examples_LICENSE "Apache-2.0")
set(RT_PUBLIC_ACCEPTANCE_ASSET_openpbr_examples_FILE_COUNT 83)
set(RT_PUBLIC_ACCEPTANCE_ASSET_openpbr_examples_BYTE_COUNT 55555)
set(RT_PUBLIC_ACCEPTANCE_ASSET_openpbr_examples_COVERAGE
    "default"
    "anisotropic_metal"
    "coat"
    "transmission_dispersion"
    "subsurface"
    "thin_film"
    "fuzz"
)
set(RT_PUBLIC_ACCEPTANCE_ASSET_openpbr_examples_FILES
    "LICENSE"
    "c71d239df91726fc519c6eb72d318ec65820627232b2f796219e87dcf35d0ab4"
    "examples/open_pbr_default.mtlx"
    "7cbd66415ef5cce2e9ce9d904761df6e8a235570bed96c33661bd4a6240add66"
    "examples/open_pbr_aluminum_brushed.mtlx"
    "02400da3191a646a6b247bee47cb885f16e481309b13d56d4dc8f45e24e6cc55"
    "examples/open_pbr_carpaint.mtlx"
    "a8e1fd94c883130fcfff979bcbc591d28570ebd42bcb2531f315fff7ed658c24"
    "examples/open_pbr_glass.mtlx"
    "5eb792195739aedd02e93b09b9c642ee26bcfee735cbdba0cbb51e551ca91c94"
    "examples/open_pbr_skin_iii.mtlx"
    "1e0cbb1171363da56aec0936f69068d241facc522377b0fd8e7f69069cfc267a"
    "examples/open_pbr_soapbubble.mtlx"
    "be07f419df42072e1c95bd469dd182535c876a9af8d9b78e001d253020481c4d"
    "examples/open_pbr_velvet.mtlx"
    "6af22cf41be2cb3176d9f179aa1077b322535e76ec38ab045da689d93e0af7a7"
)
