fn main(){

}


#[allow(bad_style)]
mod lib;

pub use crate::lib::*;
use std::ptr; 
use image::*;

#[test]
fn encoder(){
      
     unsafe {
      let img = image::open("./tests/lab.jpg").unwrap();

      let dimentsion = img.dimensions();
  
      let img16 = img.into_rgb8();
      let data = img16.into_raw() as Vec<u8>;

      let mut params: grk_cparameters =  std::mem::uninitialized();
      let color_space = GRK_CLRSPC_SRGB;
      let numComps: u32 = 3;

      grk_compress_set_default_params(&mut params);

      let mut cmptparm: grk_image_cmptparm = std::mem::uninitialized();
      cmptparm.prec = 8;
      cmptparm.sgnd = true;
      cmptparm.dx = 0;
      cmptparm.dy = 0;
      cmptparm.w = 0;
      cmptparm.h = 0;

      let image = grk_image_new(numComps as u16, &mut cmptparm, color_space,true);
       let mut imgx = *image;

      for i in 0..cmptparm.w*cmptparm.h {
            let mut j: u32 = 0;
            for j in 0..numComps {
                  let index = (i * 3 + j) as usize;
                  let comps = (*image).comps;
                  let comp_data = comps.offset(j as isize);
                  let value = data[index];
                  *(*comp_data).data.offset(i as isize) = value as i32;
            }
      }

      let mut out_stream = std::mem::uninitialized();

      let out_stream_ptr: *mut u8 = &mut out_stream;

      let stream = grk_stream_create_mem_stream(out_stream_ptr, 1024*1024, false, true);
      let codec = grk_compress_create(GRK_CODEC_JP2, stream);
      grk_compress_init(codec, &mut params, image);
      let mut bSuccess = grk_compress_start(codec);

      if( !bSuccess ) {
            grk_object_unref(stream);
            grk_object_unref(codec);
            grk_object_unref(&mut (*image).obj);
        }
        bSuccess = grk_compress(codec);
        bSuccess = grk_compress_end(codec);

       grk_object_unref(stream);

       grk_object_unref(codec);
       grk_object_unref(&mut (*image).obj);
     }
}