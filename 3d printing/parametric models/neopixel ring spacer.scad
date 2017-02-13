height = 2;
radius = 34;
width = 7;
slot_width=15;
resol = 180;
difference(){
    cylinder(h=height,r=radius,center=true,$fn=resol);
    cylinder(h=height,r=radius-width,center=true,$fn=resol);
    translate([0,-slot_width/2,-height/2]){
        cube([radius,slot_width,height]);    
    }
}