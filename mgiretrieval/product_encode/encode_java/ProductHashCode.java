public class ProductHashCode {
    public static void main(String[] args) {
        // 检查是否提供了产品名称作为命令行参数
        if (args.length < 1) {
            System.out.println("Please provide a product name as an argument.");
            return;
        }

        // 获取产品名称
        String productName = args[0];

        // 计算哈希码
        int hashCode = Math.abs(productName.hashCode());

        // 输出哈希码
        // System.out.println("The hash code of the product_name:'" + productName + "' is:" + hashCode);
        System.out.println(productName + ":" + hashCode);
    }
}
