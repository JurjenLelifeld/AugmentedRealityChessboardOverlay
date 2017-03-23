This is a simple and quick implementation of a augmented reality overlay. This C++ application uses OpenCV to insert an image on a chessboard that is shown in a video feed. The image will follow the chessboard no matter the angle or the rotation, as long as the chessboard is completely visible. Please note that moving the chessboard to fast will create a blur which makes it impossible to detect the chessboard. I hope to make this application better in the future!

Some examples:

*Original input frame*
<img src="Pictures/original_frame_example.png" style="width: 500px;"/>

*Chessboard drawing over the frame when chessboard is detected*
<img src="Pictures/chessboard_drawing_example.png" style="width: 500px;"/>

*The input image is put over the chessboard no matter the position*
<img src="Pictures/processed_images_example.gif" style="width: 500px;"/>