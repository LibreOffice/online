
// Entry file for the rollup bundle - this lists the files to be bundled together.

// Upstream CSS from own repo
import '../css/w2ui-1.5.rc1.css';
import '../css/leaflet.css';
import '../css/leaflet-spinner.css';
import '../css/selectionMarkers.css';
import '../css/loleaflet.css';
import '../css/toolbar.css';
import '../css/partsPreviewControl.css';
import '../css/scrollBar.css';
import '../css/searchControl.css';
import '../css/spreadsheet.css';
import '../css/editor.css';
import '../css/jquery.mCustomScrollbar.css';
import '../css/menubar.css';

// Upstream CSS from NPM modules
import 'select2/dist/css/select2.css';
import 'jquery-contextmenu/dist/jquery.contextMenu.css';
import 'vex-js/dist/css/vex.css';
import 'vex-js/dist/css/vex-theme-plain.css';
import 'vex-js/dist/css/vex-theme-bottom-right-corner.css';
import 'smartmenus/dist/css/sm-core-css.css';
import 'smartmenus/dist/css/sm-simple/sm-simple.css';
import 'jquery-ui/themes/ui-lightness/jquery-ui.css';


// Upstream JS from NPM modules
import 'hammerjs/hammer.js';
import 'jquery/dist/jquery.js';
import 'jquery-mousewheel/jquery.mousewheel.js';
import 'jquery-contextmenu/dist/jquery.contextMenu.js';
import 'jquery-ui/jquery-ui.js';
import 'smartmenus/dist/jquery.smartmenus.js';
import 'autolinker/dist/Autolinker.js';
import 'json-js/json2.js';
import 'select2/dist/js/select2.js';
import 'vex-js/dist/js/vex.combined.js';

// sanitize-url is import'ed just frmo control/Toolbar.js and control/Control.AlertDialog.js
// import '@braintree/sanitize-url/index.js';

// Upstream JS from own repo
// mCustomScrollbar is unmaintained and has custom hacks
import '../js/jquery.mCustomScrollbar.js';
// w2ui v1.5.rc1 is not (yet) released into NPM
import { w2utils, w2ui } from '../js/w2ui-1.5.rc1.js';
// import '../js/w2utils.js';
// import '../js/w2toolbar.js';


// loleaflet source files, in order
import './Leaflet.js';
import './errormessages.js';
import './unocommands.js';
import './viamapi-client.js';
import './core/Log.js';
import './core/Util.js';
import './core/LOUtil.js';
import './core/Class.js';
import './core/Events.js';
import './core/Socket.js';
import './core/Browser.js';
import './core/Matrix.js';
import './geometry/Point.js';
import './geometry/Bounds.js';
import './geometry/Transformation.js';
import './dom/DomUtil.js';
import './geo/LatLng.js';
import './geo/LatLngBounds.js';
import './geo/projection/Projection.LonLat.js';
import './geo/crs/CRS.js';
import './geo/crs/CRS.Simple.js';
import './map/Map.js';
import './layer/Layer.js';
import './layer/tile/GridLayer.js';
import './layer/tile/TileLayer.js';
import './layer/tile/TileLayer.WMS.js';
import './layer/tile/WriterTileLayer.js';
import './layer/tile/ImpressTileLayer.js';
import './layer/tile/CalcTileLayer.js';
import './layer/ImageOverlay.js';
import './layer/marker/ProgressOverlay.js';
import './layer/marker/ClipboardContainer.js';
import './layer/marker/Icon.js';
import './layer/marker/Icon.Default.js';
import './layer/marker/Marker.js';
import './layer/marker/Cursor.js';
import './layer/marker/DivIcon.js';
import './layer/Popup.js';
import './layer/Layer.Popup.js';
import './layer/marker/Marker.Popup.js';
import './layer/LayerGroup.js';
import './layer/FeatureGroup.js';
import './layer/vector/Renderer.js';
import './layer/vector/Path.js';
import './layer/vector/Path.Popup.js';
import './geometry/LineUtil.js';
import './layer/vector/Polyline.js';
import './geometry/PolyUtil.js';
import './layer/vector/Polygon.js';
import './layer/vector/Rectangle.js';
import './layer/vector/CircleMarker.js';
import './layer/vector/Circle.js';
import './layer/vector/SVG.js';
import './layer/vector/Path.Transform.SVG.js';
import './core/Handler.js';
import './layer/vector/SVGGroup.js';
import './layer/vector/Path.Drag.Transform.js';
import './layer/vector/Path.Drag.js';
import './layer/vector/Path.Transform.Util.js';
import './layer/vector/Path.Transform.js';
import './layer/vector/SVG.VML.js';
import './layer/vector/Path.Transform.SVG.VML.js';
import './layer/vector/Canvas.js';
import './layer/vector/Path.Transform.Canvas.js';
// import './layer/GeoJSON.js';
import './dom/DomEvent.js';
import './dom/Draggable.js';
import './map/handler/Map.Drag.js';
import './map/handler/Map.Scroll.js';
import './map/handler/Map.DoubleClickZoom.js';
import './dom/DomEvent.DoubleTap.js';
import './dom/DomEvent.Pointer.js';
import './map/handler/Map.TouchZoom.js';
import './map/handler/Map.Tap.js';
import './map/handler/Map.BoxZoom.js';
import './map/handler/Map.Keyboard.js';
import './dom/DomEvent.MultiClick.js';
import './map/handler/Map.Mouse.js';
import './map/handler/Map.Print.js';
import './map/handler/Map.SlideShow.js';
import './map/handler/Map.FileInserter.js';
import './map/handler/Map.StateChanges.js';
import './map/handler/Map.WOPI.js';
import './layer/marker/Marker.Drag.js';
import './control/Control.Toolbar.js';
import './control/Control.js';
import './control/Control.PartsPreview.js';
import './control/Control.Header.js';
import './control/Control.ColumnHeader.js';
import './control/Control.MobileInput.js';
import './control/Control.ContextToolbar.js';
import './control/Control.RowHeader.js';
import './control/Control.DocumentRepair.js';
import './control/Control.ContextMenu.js';
import './control/Control.Menubar.js';
import './control/Control.Tabs.js';
import './control/Control.Permission.js';
import './control/Control.Selection.js';
import './control/Control.Scroll.js';
import './control/Control.LokDialog.js';
import './control/Control.AlertDialog.js';
import './control/Control.Infobar.js';
import './control/Control.Attribution.js';
import './control/Control.Scale.js';
import './control/Control.Layers.js';
import './control/Search.js';
import './control/Permission.js';
import './control/Toolbar.js';
import './control/Signing.js';
import './control/Parts.js';
import './control/Scroll.js';
import './control/Styles.js';
import './control/Ruler.js';
import './dom/PosAnimation.js';
import './map/anim/Map.PanAnimation.js';
import './dom/PosAnimation.Timer.js';
import './map/anim/Map.ZoomAnimation.js';
import './map/anim/Map.FlyTo.js';
import './layer/AnnotationManager.js';
import './control/Control.Scroll.Annotation.js';
import './layer/marker/Annotation.js';
import './layer/marker/DivOverlay.js';

import './main.js';
