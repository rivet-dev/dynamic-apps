export default {
	fetch(request: Request) {
		return Response.json({
			message: "Replace this seed with the generated application.",
			path: new URL(request.url).pathname,
		});
	},
};
