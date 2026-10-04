class Solution {
    public int trap(int[] height) {
        int water = 0;
        int wall = 0;
        for (int i = 0; i < height.length; i++) {
            if (height[i] >= wall) {
                wall = height[i];
            } else {
                water += wall - height[i];
            }
        }
        return water;
    }
}