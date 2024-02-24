import bpy
import json
import math

def print(data):
    for window in bpy.context.window_manager.windows:
        screen = window.screen
        for area in screen.areas:
            if area.type == 'CONSOLE':
                override = {'window': window, 'screen': screen, 'area': area}
                bpy.ops.console.scrollback_append(override, text=str(data), type="OUTPUT")




def import_objects(filepath, texturedir, ignore_transform = False):
    # Opening JSON file
    f = open(filepath)
     
    # returns JSON object as 
    # a dictionary
    data = json.load(f)

    splitindex = 0
    collection = bpy.data.collections.new("Imported Objects")
    bpy.context.scene.collection.children.link(collection)

    maxsplit = 0
     
    # Iterating through the json
    # list
    for i, split in data['object_splits'].items():
        vertices = []
        edges = []
        faces = []
        for v in split['vertices']:
            # From (X, Y, Z) to (X, -Z, Y)
            vertices.append((v[0] / 100.0, v[2]  / -100.0, v[1]  / 100.0))
        index = 0
        for face in split['faces']:
            faces.append((face[0], face[1], face[2]))
            index += 3
        mesh = bpy.data.meshes.new(name = "GameObjMesh" + str(splitindex))
        mesh.from_pydata(vertices, edges, faces)
        
        # Create a mesh object
        obj = bpy.data.objects.new("GameObjObject" + str(i), mesh)
        splitindex += 1
        
        # Apply world transform if necessary
        if not ignore_transform:
            obj.location[0] = split['position'][0] / 100.0
            obj.location[1] = split['position'][2] / -100.0
            obj.location[2] = split['position'][1] / 100.0
            obj.scale[0] = split['scale'][0]
            obj.scale[1] = split['scale'][2]
            obj.scale[2] = split['scale'][1]
            obj.rotation_euler[0] = math.radians(split['rotation'][0])
            obj.rotation_euler[2] = math.radians(split['rotation'][1])
        
        # Create a new collection and add object
        collection.objects.link(obj)

        # Create and load UVs
        uvnames = [ "UVBase", "UVAlpha1", "UVColor1", "UVAlpha2", "UVColor2" ]
        # Base
        uvbase = obj.data.uv_layers.new(name=uvnames[0])
        vertloops = {}
        for loop in obj.data.loops:
            uvbase.uv[loop.index].vector.x = split['uvbase'][loop.vertex_index][0]
            uvbase.uv[loop.index].vector.y = -split['uvbase'][loop.vertex_index][1]
        # Alpha1
        uvalpha1 = obj.data.uv_layers.new(name=uvnames[1])
        for loop in obj.data.loops:
            uvalpha1.uv[loop.index].vector.x = split['uvalpha1'][loop.vertex_index][0]
            uvalpha1.uv[loop.index].vector.y = -split['uvalpha1'][loop.vertex_index][1]
        # Color1
        uvcolor1 = obj.data.uv_layers.new(name=uvnames[2])
        for loop in obj.data.loops:
            uvcolor1.uv[loop.index].vector.x = split['uvcolor1'][loop.vertex_index][0]
            uvcolor1.uv[loop.index].vector.y = -split['uvcolor1'][loop.vertex_index][1]
        # Alpha2
        uvalpha2 = obj.data.uv_layers.new(name=uvnames[3])
        for loop in obj.data.loops:
            uvalpha2.uv[loop.index].vector.x = split['uvalpha2'][loop.vertex_index][0]
            uvalpha2.uv[loop.index].vector.y = -split['uvalpha2'][loop.vertex_index][1]
        # Color2
        uvcolor2 = obj.data.uv_layers.new(name=uvnames[4])
        for loop in obj.data.loops:
            uvcolor2.uv[loop.index].vector.x = split['uvcolor2'][loop.vertex_index][0]
            uvcolor2.uv[loop.index].vector.y = -split['uvcolor2'][loop.vertex_index][1]
        
        # Create the material
        mat = bpy.data.materials.new(name = "GameObjMaterial" + str(splitindex))
        mat.use_nodes = True
        links = mat.node_tree.links
        
        # Setup material nodes
        bsdf = mat.node_tree.nodes["Principled BSDF"]
        bsdf.inputs["Specular"].default_value = 0.0
        
        # Create texture nodes
        texcount = 1
        if split['textures'][4] != "":
            texcount = 5
        elif split['textures'][2] != "":
            texcount = 3
        texnodes = []
        for i in range(texcount):
            tex = mat.node_tree.nodes.new("ShaderNodeTexImage")
            imgname = split['textures'][i]
            if imgname:
                image = bpy.data.images.get(imgname)
                if not image:
                    image = bpy.data.images.load(filepath = texturedir + "/" + imgname)
                tex.image = image
            # Create UV node
            uvnode = mat.node_tree.nodes.new("ShaderNodeUVMap")
            uvnode.uv_map = uvnames[i]
            # Link UV to texture
            links.new(tex.inputs[0], uvnode.outputs[0])
            texnodes.append(tex)
        
        
        # Setup alpha blend
        if split['alphablend']:
            mat.blend_method = 'BLEND'
            mat.show_transparent_back = split['showback']
            links.new(bsdf.inputs['Alpha'], texnodes[0].outputs[1])
        
        finalcolor = None
        if texcount == 1:
            finalcolor = texnodes[0]
        elif texcount >= 3:
            # Alpha blend base and color1 with a MixRGB
            mix = mat.node_tree.nodes.new("ShaderNodeMixRGB")
            links.new(mix.inputs[0], texnodes[1].outputs[0])
            links.new(mix.inputs[1], texnodes[0].outputs[0])
            links.new(mix.inputs[2], texnodes[2].outputs[0])
            finalcolor = mix
        if texcount == 5:
            # Alpha blend base + color1 and color2 with a MixRGB
            mix = mat.node_tree.nodes.new("ShaderNodeMixRGB")
            links.new(mix.inputs[0], texnodes[3].outputs[0])
            links.new(mix.inputs[1], finalcolor.outputs[0])
            links.new(mix.inputs[2], texnodes[4].outputs[0])
            finalcolor = mix
        
        # Link final color output to base color
        links.new(bsdf.inputs["Base Color"], finalcolor.outputs[0])
        
        # Assign material
        obj.active_material = mat
        
        if maxsplit != 0 and splitindex >= maxsplit:
            break;
     
    # Closing file
    f.close()


# Usage:
#import_objects("D:/ArchLord_Main/archlord-editor/content/exports/Object132646586.json", 
#    "D:/ArchLord_Main/content/textures/object", False)
