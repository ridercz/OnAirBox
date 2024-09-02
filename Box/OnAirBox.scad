/********************************************************************************************************************
 * On-Air Indicator Box                                                                                             *
 * ---------------------------------------------------------------------------------------------------------------- *
 * Copyright (c) Michal Altair Valasek, 2024 | www.rider.cz | github.com/ridercz                                    *
 * Licensed under terms of the Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License.     *
 * ---------------------------------------------------------------------------------------------------------------- *
 * This is a simple box for an on-air indicator. It has a hole for a LED or a button with LED, a hole for USB       *
 * connector and a text on the front side. The lid is designed to be press-fit to the box.                          *
 ********************************************************************************************************************/

include <A2D.scad>; // https://github.com/ridercz/A2D
assert(a2d_required([1, 6, 2]), "Please upgrade A2D library to version 1.6.2 or higher.");

/* [General] */
main_hole_diameter = 16;
main_hole_lip_thickness  = 0;
main_hole_lip_height = 0;
inner_size = [100, 35, 25];
corner_radius = 3;
face_thickness = 1;
lid_thickness = 2; 

/* [USB connector hole] */
usb_width = 10;
usb_height = 8;
usb_offset_y = 0;

/* [Text] */
text_face_string = "ON AIR";
text_face_size = 12;
text_face_offset = -6;
text_face_font = "Arial:bold";
text_face_extr = .4;
text_lid_string = ["MASTER/R02", "RIDER.CZ 2024"];
text_lid_size = 6;
text_lid_font = "Arial:bold";
text_lid_extr = .4;

/* [Hidden] */
$fudge = 1;
$fn = 32;
outer_size = inner_size + [2 * corner_radius, 2 * corner_radius, 2 * face_thickness + lid_thickness];

echo("Outer size: ", outer_size);

/* Render ***********************************************************************************************************/

translate([0, outer_size.y + 3]) part_box();
part_lid();

/* Parts ************************************************************************************************************/

module part_box() {
    difference() {
        // The box itself
        union() {
            linear_extrude(height = face_thickness) r_square([outer_size.x, outer_size.y], corner_radius);
            linear_extrude(height = outer_size.z - face_thickness) rh_square([outer_size.x, outer_size.y], radius = corner_radius, thickness = -corner_radius);
        }

        // LED or button hole
        translate(v = [outer_size.y / 2, outer_size.y / 2, -$fudge]) cylinder(d = main_hole_diameter, h = face_thickness + 2 * $fudge);

        // USB connector hole
        translate(v = [outer_size.x - corner_radius - $fudge, (outer_size.y - usb_width) / 2 + usb_offset_y, outer_size.z - usb_height - lid_thickness]) cube([corner_radius + 2 * $fudge, usb_width, usb_height + lid_thickness + $fudge]);

        // Text
        translate(v = [outer_size.y + text_face_offset, outer_size.y / 2, -$fudge]) linear_extrude(height = $fudge + text_face_extr) mirror(v = [0, 1, 0]) text(text = text_face_string, size = text_face_size, font = text_face_font, halign = "left", valign = "center");
    }

    // LED lip
    if(main_hole_lip_height > 0 && main_hole_lip_thickness > 0) translate(v = [outer_size.y / 2, outer_size.y / 2]) linear_extrude(height = main_hole_lip_height) h_circle(d = main_hole_diameter, thickness = main_hole_lip_thickness);
}

module part_lid() {
    difference() {
        union() {
            // Bottom part
            linear_extrude(height = face_thickness) r_square([outer_size.x, outer_size.y], corner_radius);

            // Interlocking part
            translate(v = [corner_radius, corner_radius]) cube([inner_size.x, inner_size.y, face_thickness + lid_thickness]);
        }
        // Text
        translate(v = [outer_size.x / 2, outer_size.y / 2, -$fudge]) linear_extrude(height = $fudge + text_lid_extr) mirror(v = [0, 1, 0]) multiline_text(text = text_lid_string, size = text_lid_size, font = text_lid_font, line_height = 1.6, halign = "center", valign = "center");
    }

}