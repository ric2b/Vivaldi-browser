
/** Copyright 2016 Vivaldi Technologies AS */

/* All the work we do onload. */
function onLoadWorkVivaldi() {
  console.log("Bar");
  $('product-license').innerHTML = loadTimeData.getString('productLicense');
  $('product-tos').innerHTML = loadTimeData.getString('productTOS');
  console.log("Bar2");
}


console.log("Fooo");
document.addEventListener('DOMContentLoaded', onLoadWorkVivaldi);
