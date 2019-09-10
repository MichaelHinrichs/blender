import bpy
from .. node_builder import NodeBuilder
from .. base import FunctionNode


class ConstantFalloffNode(bpy.types.Node, FunctionNode):
    bl_idname = "fn_ConstantFalloffNode"
    bl_label = "Constant Falloff"

    def declaration(self, builder: NodeBuilder):
        builder.fixed_input("weight", "Weight", "Float", default=1.0)
        builder.fixed_output("falloff", "Falloff", "Falloff")


class PointDistanceFalloffNode(bpy.types.Node, FunctionNode):
    bl_idname = "fn_PointDistanceFalloffNode"
    bl_label = "Point Distance Falloff"

    def declaration(self, builder: NodeBuilder):
        builder.fixed_input("point", "Point", "Vector")
        builder.fixed_input("min_distance", "Min Distance", "Float")
        builder.fixed_input("max_distance", "Max Distance", "Float", default=1)
        builder.fixed_output("falloff", "Falloff", "Falloff")


class MeshDistanceFalloffNode(bpy.types.Node, FunctionNode):
    bl_idname = "fn_MeshDistanceFalloffNode"
    bl_label = "Mesh Distance Falloff"

    def declaration(self, builder: NodeBuilder):
        builder.fixed_input("object", "Object", "Object")
        builder.fixed_input("inner_distance", "Inner Distance", "Float")
        builder.fixed_input("outer_distance", "Outer Distance", "Float", default=1.0)
        builder.fixed_output("falloff", "Falloff", "Falloff")
