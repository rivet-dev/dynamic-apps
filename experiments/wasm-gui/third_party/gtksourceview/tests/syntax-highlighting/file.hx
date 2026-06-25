/* Multi line comment
 * ==================
 * line 1
 * line 2
 * */

package;

import flash.display.Sprite;

class Main extends Sprite
{
    var foo:Int = 400; // single line comment
    public static function main():Void
    {
        @metadata("argument")
        @:buildMacro
        var boolean:Bool = false;
    }
}
