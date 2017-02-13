rCal = 40;
hCal = 25;
hBase = 0.5;
radius = ((rCal*rCal)+(hCal*hCal))/(2*hCal);
thikness = 1;
resol = 180;
sphere_height = radius-hCal;
translate([0,0,-sphere_height]){
    difference(){
        sphere(r = radius,$fn=resol);
        sphere(r = radius-thikness,$fn=resol);    
        translate([0,0,-hCal]){
            cube(radius*2,center=true);
        }
    }
}
translate([0,0,0]){
    difference(){
        cylinder(hBase,r=rCal+1,center=true,$fn=resol);
        cylinder(hBase,r=rCal,center=true,$fn=resol);
    }
}