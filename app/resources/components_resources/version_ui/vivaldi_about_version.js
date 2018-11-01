
/** Copyright 2016 Vivaldi Technologies AS */

/* All the work we do onload. */
function onLoadWorkVivaldi() {
  $('product-license').innerHTML = loadTimeData.getString('productLicense');
  $('product-tos').innerHTML = loadTimeData.getString('productTOS');
}


document.addEventListener('DOMContentLoaded', onLoadWorkVivaldi);
