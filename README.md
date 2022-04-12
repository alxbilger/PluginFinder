# PluginFinder

PluginFinder is a program loading SOFA simulations in order to find the required SOFA plugins, i.e. the plugins containing all the components of the simulation.

- SOFA: [https://www.sofa-framework.org/](https://www.sofa-framework.org/)

## How it works

It first loads all the SOFA plugins from the list contained in the file `plugin_list.conf` or `plugin_list.conf.default`.
It is a similar mechanism in `runSofa`.
In order to find the required plugins, all the plugins must be loaded.
:warning: A plugin cannot be found if it is not loaded.

It loads one or several simulations sequentially.
For each simulation:
- The simulation is loaded
- All the components (derived from `sofa::core::objectmodel::BaseObject`) are retrieved from the root node
- For each component, it finds the corresponding plugin. The component must be found in the `sofa::core::ObjectFactory`, i.e. the corresponding plugin must be loaded.
- Replace the `<RequiredPlugin/>` list by the new one.
- The simulation is unloaded

## Limitations

- A plugin cannot be found if it is not loaded
- Limited to XML files
- At least one `<RequiredPlugin/>` must be present in the scene

## Demo

```
PluginFinder examples\Components\mass\DiagonalMass.scn
```

The following scene

```xml
<?xml version="1.0" ?>
<Node name="root" dt="0.005">
    <RequiredPlugin name="Sofa.Component.Mass"/>

    <DefaultPipeline verbose="0" name="CollisionPipeline" />
    <BruteForceBroadPhase/>
    <BVHNarrowPhase/>
    <DefaultContactManager response="PenalityContactForceField" name="collision response" />
    <DiscreteIntersection />

    <MeshGmshLoader name="loader" filename="mesh/liver.msh" />
    <MeshOBJLoader name="meshLoader_0" filename="mesh/liver-smooth.obj" handleSeams="1" />

    <Node name="Liver" depend="topo dofs">
        <EulerImplicitSolver name="integration scheme" />
        <CGLinearSolver name="linear solver" iterations="1000" tolerance="1e-9" threshold="1e-9"/>
        <MechanicalObject name="dofs" src="@../loader" />
        <!-- Container for the tetrahedra-->
        <TetrahedronSetTopologyContainer name="TetraTopo" src="@../loader" />
        <TetrahedronSetGeometryAlgorithms name="GeomAlgo" />
        <DiagonalMass totalMass="60" name="diagonalMass" />
        <TetrahedralCorotationalFEMForceField template="Vec3d" name="FEM" method="large" poissonRatio="0.45" youngModulus="5000" />
        <FixedConstraint name="FixedConstraint" indices="3 39 64" />
        
        <Node name="Visu">
            <OglModel name="VisualModel" src="@../../meshLoader_0" color="red" />
            <BarycentricMapping name="VisualMapping" input="@../dofs" output="@VisualModel" />
        </Node>
        <Node name="Surf">
    	    <SphereLoader filename="mesh/liver.sph" />
            <MechanicalObject name="spheres" position="@[-1].position" />
            <SphereCollisionModel name="CollisionModel" listRadius="@[-2].listRadius" />
            <BarycentricMapping name="CollisionMapping" input="@../dofs" output="@spheres" />
        </Node>
    </Node>
</Node>
```

is transformed to:

```xml
<?xml version="1.0" ?>
<Node name="root" dt="0.005">
    <RequiredPlugin name="Sofa.Component.Constraint.Projective"/> <!-- Needed to use components [FixedConstraint] -->
    <RequiredPlugin name="Sofa.Component.LinearSolver.Iterative"/> <!-- Needed to use components [CGLinearSolver] -->
    <RequiredPlugin name="Sofa.Component.Mapping.Linear"/> <!-- Needed to use components [BarycentricMapping] -->
    <RequiredPlugin name="Sofa.Component.Mass"/> <!-- Needed to use components [DiagonalMass] -->
    <RequiredPlugin name="Sofa.Component.ODESolver.Backward"/> <!-- Needed to use components [EulerImplicitSolver] -->
    <RequiredPlugin name="Sofa.Component.SolidMechanics.FEM.Elastic"/> <!-- Needed to use components [TetrahedralCorotationalFEMForceField] -->
    <RequiredPlugin name="Sofa.Component.StateContainer"/> <!-- Needed to use components [MechanicalObject] -->
    <RequiredPlugin name="Sofa.GL.Component.Rendering3D"/> <!-- Needed to use components [OglModel] -->

    <DefaultPipeline verbose="0" name="CollisionPipeline" />
    <BruteForceBroadPhase/>
    <BVHNarrowPhase/>
    <DefaultContactManager response="PenalityContactForceField" name="collision response" />
    <DiscreteIntersection />

    <MeshGmshLoader name="loader" filename="mesh/liver.msh" />
    <MeshOBJLoader name="meshLoader_0" filename="mesh/liver-smooth.obj" handleSeams="1" />

    <Node name="Liver" depend="topo dofs">
        <EulerImplicitSolver name="integration scheme" />
        <CGLinearSolver name="linear solver" iterations="1000" tolerance="1e-9" threshold="1e-9"/>
        <MechanicalObject name="dofs" src="@../loader" />
        <!-- Container for the tetrahedra-->
        <TetrahedronSetTopologyContainer name="TetraTopo" src="@../loader" />
        <TetrahedronSetGeometryAlgorithms name="GeomAlgo" />
        <DiagonalMass totalMass="60" name="diagonalMass" />
        <TetrahedralCorotationalFEMForceField template="Vec3d" name="FEM" method="large" poissonRatio="0.45" youngModulus="5000" />
        <FixedConstraint name="FixedConstraint" indices="3 39 64" />
        
        <Node name="Visu">
            <OglModel name="VisualModel" src="@../../meshLoader_0" color="red" />
            <BarycentricMapping name="VisualMapping" input="@../dofs" output="@VisualModel" />
        </Node>
        <Node name="Surf">
    	    <SphereLoader filename="mesh/liver.sph" />
            <MechanicalObject name="spheres" position="@[-1].position" />
            <SphereCollisionModel name="CollisionModel" listRadius="@[-2].listRadius" />
            <BarycentricMapping name="CollisionMapping" input="@../dofs" output="@spheres" />
        </Node>
    </Node>
</Node>
```
