override ll: u32 = 0;
var<workgroup> oob: array<u32, ll>;

@compute @workgroup_size(1)
fn main() {
    let x = oob[0];
}
